#define _GNU_SOURCE  // for asprintf()
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include "jsonpull.h"

#define BUFFER 10000

json_pull *json_begin(ssize_t (*read)(struct json_pull *, char *buffer, size_t n), void *source) {
	json_pull *j = malloc(sizeof(json_pull));
	if (j == NULL) {
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}

	j->error = NULL;
	j->line = 1;
	j->container = NULL;
	j->root = NULL;

	j->read = read;
	j->source = source;
	j->buffer_head = 0;
	j->buffer_tail = 0;

	j->buffer = malloc(BUFFER);
	if (j->buffer == NULL) {
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}

	return j;
}

static inline int peek(json_pull *j) {
	if (j->buffer_head < j->buffer_tail) {
		return (unsigned char) j->buffer[j->buffer_head];
	} else {
		j->buffer_head = 0;
		j->buffer_tail = j->read(j, j->buffer, BUFFER);
		if (j->buffer_head >= j->buffer_tail) {
			return EOF;
		}
		return (unsigned char) j->buffer[j->buffer_head];
	}
}

static inline int next(json_pull *j) {
	if (j->buffer_head < j->buffer_tail) {
		return (unsigned char) j->buffer[j->buffer_head++];
	} else {
		j->buffer_head = 0;
		j->buffer_tail = j->read(j, j->buffer, BUFFER);
		if (j->buffer_head >= j->buffer_tail) {
			return EOF;
		}
		return (unsigned char) j->buffer[j->buffer_head++];
	}
}

static ssize_t read_file(json_pull *j, char *buffer, size_t n) {
	return fread(buffer, 1, n, j->source);
}

json_pull *json_begin_file(FILE *f) {
	return json_begin(read_file, f);
}

static ssize_t read_string(json_pull *j, char *buffer, size_t n) {
	const char *cp = j->source;
	size_t out = 0;

	while (out < n && cp[out] != '\0') {
		buffer[out] = cp[out];
		out++;
	}

	j->source = (void *) (cp + out);
	return out;
}

json_pull *json_begin_string(const char *s) {
	return json_begin(read_string, (void *) s);
}

void json_end(json_pull *p) {
	json_free(p->root);
	free(p->buffer);
	free(p);
}

static inline int read_wrap(json_pull *j) {
	int c = next(j);

	if (c == '\n') {
		j->line++;
	}

	return c;
}

#define SIZE_FOR(i, size) ((size_t)((((i) + 31) & ~31) * size))

static json_object *fabricate_object(json_pull *jp, json_object *parent, json_type type) {
	json_object *o = malloc(sizeof(struct json_object));
	if (o == NULL) {
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}
	o->type = type;
	o->parent = parent;
	o->array = NULL;
	o->keys = NULL;
	o->values = NULL;
	o->length = 0;
	o->parser = jp;
	return o;
}

static json_object *add_object(json_pull *j, json_type type) {
	json_object *c = j->container;
	json_object *o = fabricate_object(j, c, type);

	if (c != NULL) {
		if (c->type == JSON_ARRAY) {
			if (c->expect == JSON_ITEM) {
				if (SIZE_FOR(c->length + 1, sizeof(json_object *)) != SIZE_FOR(c->length, sizeof(json_object *))) {
					if (SIZE_FOR(c->length + 1, sizeof(json_object *)) < SIZE_FOR(c->length, sizeof(json_object *))) {
						fprintf(stderr, "Array size overflow\n");
						exit(EXIT_FAILURE);
					}
					c->array = realloc(c->array, SIZE_FOR(c->length + 1, sizeof(json_object *)));
					if (c->array == NULL) {
						perror("Out of memory");
						exit(EXIT_FAILURE);
					}
				}

				c->array[c->length++] = o;
				c->expect = JSON_COMMA;
			} else {
				j->error = "Expected a comma, not a list item";
				free(o);
				return NULL;
			}
		} else if (c->type == JSON_HASH) {
			if (c->expect == JSON_VALUE) {
				c->values[c->length - 1] = o;
				c->expect = JSON_COMMA;
			} else if (c->expect == JSON_KEY) {
				if (type != JSON_STRING) {
					j->error = "Hash key is not a string";
					free(o);
					return NULL;
				}

				if (SIZE_FOR(c->length + 1, sizeof(json_object *)) != SIZE_FOR(c->length, sizeof(json_object *))) {
					if (SIZE_FOR(c->length + 1, sizeof(json_object *)) < SIZE_FOR(c->length, sizeof(json_object *))) {
						fprintf(stderr, "Hash size overflow\n");
						exit(EXIT_FAILURE);
					}
					c->keys = realloc(c->keys, SIZE_FOR(c->length + 1, sizeof(json_object *)));
					c->values = realloc(c->values, SIZE_FOR(c->length + 1, sizeof(json_object *)));
					if (c->keys == NULL || c->values == NULL) {
						perror("Out of memory");
						exit(EXIT_FAILURE);
					}
				}

				c->keys[c->length] = o;
				c->values[c->length] = NULL;
				c->length++;
				c->expect = JSON_COLON;
			} else {
				j->error = "Expected a comma or colon";
				free(o);
				return NULL;
			}
		}
	} else {
		if (j->root != NULL) {
			json_free(j->root);
		}

		j->root = o;
	}

	return o;
}

json_object *json_hash_get(json_object *o, const char *s) {
	if (o == NULL || o->type != JSON_HASH) {
		return NULL;
	}

	size_t i;
	for (i = 0; i < o->length; i++) {
		if (o->keys[i] != NULL && o->keys[i]->type == JSON_STRING) {
			if (strcmp(o->keys[i]->string, s) == 0) {
				return o->values[i];
			}
		}
	}

	return NULL;
}

struct string {
	char *buf;
	size_t n;
	size_t nalloc;
};

static void string_init(struct string *s) {
	s->nalloc = 500;
	s->buf = malloc(s->nalloc);
	if (s->buf == NULL) {
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}
	s->n = 0;
	s->buf[0] = '\0';
}

static void string_append(struct string *s, char c) {
	if (s->n + 2 >= s->nalloc) {
		size_t prev = s->nalloc;
		s->nalloc += 500;
		if (s->nalloc <= prev) {
			fprintf(stderr, "String size overflowed\n");
			exit(EXIT_FAILURE);
		}
		s->buf = realloc(s->buf, s->nalloc);
		if (s->buf == NULL) {
			perror("Out of memory");
			exit(EXIT_FAILURE);
		}
	}

	s->buf[s->n++] = c;
	s->buf[s->n] = '\0';
}

static void string_append_string(struct string *s, char *add) {
	size_t len = strlen(add);

	if (s->n + len + 1 >= s->nalloc) {
		size_t prev = s->nalloc;
		s->nalloc += 500 + len;
		if (s->nalloc <= prev) {
			fprintf(stderr, "String size overflowed\n");
			exit(EXIT_FAILURE);
		}
		s->buf = realloc(s->buf, s->nalloc);
		if (s->buf == NULL) {
			perror("Out of memory");
			exit(EXIT_FAILURE);
		}
	}

	for (; *add != '\0'; add++) {
		s->buf[s->n++] = *add;
	}

	s->buf[s->n] = '\0';
}

static void string_free(struct string *s) {
	free(s->buf);
}

json_object *json_read_separators(json_pull *j, json_separator_callback cb, void *state) {
	int c;

	// In case there is an error at the top level
	if (j->container == NULL) {
		if (j->root != NULL) {
			json_free(j->root);
		}

		j->root = NULL;
	}

again:
	/////////////////////////// Whitespace

	do {
		c = read_wrap(j);
		if (c == EOF) {
			if (j->container != NULL) {
				j->error = "Reached EOF without all containers being closed";
			}

			return NULL;
		}

		// Byte-order mark
		if (c == 0xEF) {
			int c2 = peek(j);
			if (c2 == 0xBB) {
				c2 = read_wrap(j);
				c2 = peek(j);
				if (c2 == 0xBF) {
					c2 = read_wrap(j);
					c = ' ';
					continue;
				}
			}
			j->error = "Corrupt byte-order mark found";
			return NULL;
		}
	} while (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == 0x1E);

	/////////////////////////// Arrays

	if (c == '[') {
		json_object *o = add_object(j, JSON_ARRAY);
		if (o == NULL) {
			return NULL;
		}
		j->container = o;
		j->container->expect = JSON_ITEM;

		if (cb != NULL) {
			cb(JSON_ARRAY, j, state);
		}

		goto again;
	} else if (c == ']') {
		if (j->container == NULL) {
			j->error = "Found ] at top level";
			return NULL;
		}

		if (j->container->type != JSON_ARRAY) {
			j->error = "Found ] not in an array";
			return NULL;
		}

		if (j->container->expect != JSON_COMMA) {
			if (!(j->container->expect == JSON_ITEM && j->container->length == 0)) {
				j->error = "Found ] without final element";
				return NULL;
			}
		}

		json_object *ret = j->container;
		j->container = ret->parent;
		return ret;
	}

	/////////////////////////// Hashes

	if (c == '{') {
		json_object *o = add_object(j, JSON_HASH);
		if (o == NULL) {
			return NULL;
		}
		j->container = o;
		j->container->expect = JSON_KEY;

		if (cb != NULL) {
			cb(JSON_HASH, j, state);
		}

		goto again;
	} else if (c == '}') {
		if (j->container == NULL) {
			j->error = "Found } at top level";
			return NULL;
		}

		if (j->container->type != JSON_HASH) {
			j->error = "Found } not in a hash";
			return NULL;
		}

		if (j->container->expect != JSON_COMMA) {
			if (!(j->container->expect == JSON_KEY && j->container->length == 0)) {
				j->error = "Found } without final element";
				return NULL;
			}
		}

		json_object *ret = j->container;
		j->container = ret->parent;
		return ret;
	}

	/////////////////////////// Null

	if (c == 'n') {
		if (read_wrap(j) != 'u' || read_wrap(j) != 'l' || read_wrap(j) != 'l') {
			j->error = "Found misspelling of null";
			return NULL;
		}

		return add_object(j, JSON_NULL);
	}

	/////////////////////////// NaN

	if (c == 'N') {
		if (read_wrap(j) != 'a' || read_wrap(j) != 'N') {
			j->error = "Found misspelling of NaN";
			return NULL;
		}

		j->error = "JSON does not allow NaN";
		return NULL;
	}

	/////////////////////////// Infinity

	if (c == 'I') {
		if (read_wrap(j) != 'n' || read_wrap(j) != 'f' || read_wrap(j) != 'i' ||
		    read_wrap(j) != 'n' || read_wrap(j) != 'i' || read_wrap(j) != 't' ||
		    read_wrap(j) != 'y') {
			j->error = "Found misspelling of Infinity";
			return NULL;
		}

		j->error = "JSON does not allow Infinity";
		return NULL;
	}

	/////////////////////////// True

	if (c == 't') {
		if (read_wrap(j) != 'r' || read_wrap(j) != 'u' || read_wrap(j) != 'e') {
			j->error = "Found misspelling of true";
			return NULL;
		}

		return add_object(j, JSON_TRUE);
	}

	/////////////////////////// False

	if (c == 'f') {
		if (read_wrap(j) != 'a' || read_wrap(j) != 'l' || read_wrap(j) != 's' || read_wrap(j) != 'e') {
			j->error = "Found misspelling of false";
			return NULL;
		}

		return add_object(j, JSON_FALSE);
	}

	/////////////////////////// Comma

	if (c == ',') {
		if (j->container != NULL) {
			if (j->container->expect != JSON_COMMA) {
				j->error = "Found unexpected comma";
				return NULL;
			}

			if (j->container->type == JSON_HASH) {
				j->container->expect = JSON_KEY;
			} else {
				j->container->expect = JSON_ITEM;
			}
		}

		if (cb != NULL) {
			cb(JSON_COMMA, j, state);
		}

		goto again;
	}

	/////////////////////////// Colon

	if (c == ':') {
		if (j->container == NULL) {
			j->error = "Found colon at top level";
			return NULL;
		}

		if (j->container->expect != JSON_COLON) {
			j->error = "Found unexpected colon";
			return NULL;
		}

		j->container->expect = JSON_VALUE;

		if (cb != NULL) {
			cb(JSON_COLON, j, state);
		}

		goto again;
	}

	/////////////////////////// Numbers

	if (c == '-' || (c >= '0' && c <= '9')) {
		struct string val;
		string_init(&val);

		if (c == '-') {
			string_append(&val, c);
			c = read_wrap(j);
		}

		if (c == '0') {
			string_append(&val, c);
		} else if (c >= '1' && c <= '9') {
			string_append(&val, c);
			c = peek(j);

			while (c >= '0' && c <= '9') {
				string_append(&val, read_wrap(j));
				c = peek(j);
			}
		}

		if (peek(j) == '.') {
			string_append(&val, read_wrap(j));

			c = peek(j);
			if (c < '0' || c > '9') {
				j->error = "Decimal point without digits";
				string_free(&val);
				return NULL;
			}
			while (c >= '0' && c <= '9') {
				string_append(&val, read_wrap(j));
				c = peek(j);
			}
		}

		c = peek(j);
		if (c == 'e' || c == 'E') {
			string_append(&val, read_wrap(j));

			c = peek(j);
			if (c == '+' || c == '-') {
				string_append(&val, read_wrap(j));
			}

			c = peek(j);
			if (c < '0' || c > '9') {
				j->error = "Exponent without digits";
				string_free(&val);
				return NULL;
			}
			while (c >= '0' && c <= '9') {
				string_append(&val, read_wrap(j));
				c = peek(j);
			}
		}

		json_object *n = add_object(j, JSON_NUMBER);
		if (n != NULL) {
			n->number = atof(val.buf);
			n->string = val.buf;
			n->length = val.n;
		} else {
			string_free(&val);
		}
		return n;
	}

	/////////////////////////// Strings

	if (c == '"') {
		struct string val;
		string_init(&val);

		int surrogate = -1;
		while ((c = read_wrap(j)) != EOF) {
			if (c == '"') {
				if (surrogate >= 0) {
					string_append(&val, 0xE0 | (surrogate >> 12));
					string_append(&val, 0x80 | ((surrogate >> 6) & 0x3F));
					string_append(&val, 0x80 | (surrogate & 0x3F));
					surrogate = -1;
				}

				break;
			} else if (c == '\\') {
				c = read_wrap(j);

				if (c == 'u') {
					char hex[5] = "aaaa";
					int i;
					for (i = 0; i < 4; i++) {
						hex[i] = read_wrap(j);
						if (hex[i] < '0' || (hex[i] > '9' && hex[i] < 'A') || (hex[i] > 'F' && hex[i] < 'a') || hex[i] > 'f') {
							j->error = "Invalid \\u hex character";
							string_free(&val);
							return NULL;
						}
					}

					unsigned long ch = strtoul(hex, NULL, 16);
					if (ch >= 0xd800 && ch <= 0xdbff) {
						if (surrogate < 0) {
							surrogate = ch;
						} else {
							// Impossible surrogate, so output the first half,
							// keep what might be a legitimate new first half.
							string_append(&val, 0xE0 | (surrogate >> 12));
							string_append(&val, 0x80 | ((surrogate >> 6) & 0x3F));
							string_append(&val, 0x80 | (surrogate & 0x3F));
							surrogate = ch;
						}
						continue;
					} else if (ch >= 0xdc00 && c <= 0xdfff) {
						if (surrogate >= 0) {
							long c1 = surrogate - 0xd800;
							long c2 = ch - 0xdc00;
							ch = ((c1 << 10) | c2) + 0x010000;
							surrogate = -1;
						}
					}

					if (surrogate >= 0) {
						string_append(&val, 0xE0 | (surrogate >> 12));
						string_append(&val, 0x80 | ((surrogate >> 6) & 0x3F));
						string_append(&val, 0x80 | (surrogate & 0x3F));
						surrogate = -1;
					}

					if (ch <= 0x7F) {
						string_append(&val, ch);
					} else if (ch <= 0x7FF) {
						string_append(&val, 0xC0 | (ch >> 6));
						string_append(&val, 0x80 | (ch & 0x3F));
					} else if (ch < 0xFFFF) {
						string_append(&val, 0xE0 | (ch >> 12));
						string_append(&val, 0x80 | ((ch >> 6) & 0x3F));
						string_append(&val, 0x80 | (ch & 0x3F));
					} else {
						string_append(&val, 0xF0 | (ch >> 18));
						string_append(&val, 0x80 | ((ch >> 12) & 0x3F));
						string_append(&val, 0x80 | ((ch >> 6) & 0x3F));
						string_append(&val, 0x80 | (ch & 0x3F));
					}
				} else {
					if (surrogate >= 0) {
						string_append(&val, 0xE0 | (surrogate >> 12));
						string_append(&val, 0x80 | ((surrogate >> 6) & 0x3F));
						string_append(&val, 0x80 | (surrogate & 0x3F));
						surrogate = -1;
					}

					if (c == '"') {
						string_append(&val, '"');
					} else if (c == '\\') {
						string_append(&val, '\\');
					} else if (c == '/') {
						string_append(&val, '/');
					} else if (c == 'b') {
						string_append(&val, '\b');
					} else if (c == 'f') {
						string_append(&val, '\f');
					} else if (c == 'n') {
						string_append(&val, '\n');
					} else if (c == 'r') {
						string_append(&val, '\r');
					} else if (c == 't') {
						string_append(&val, '\t');
					} else {
						j->error = "Found backslash followed by unknown character";
						string_free(&val);
						return NULL;
					}
				}
			} else if (c < ' ') {
				j->error = "Found control character in string";
				string_free(&val);
				return NULL;
			} else {
				if (surrogate >= 0) {
					string_append(&val, 0xE0 | (surrogate >> 12));
					string_append(&val, 0x80 | ((surrogate >> 6) & 0x3F));
					string_append(&val, 0x80 | (surrogate & 0x3F));
					surrogate = -1;
				}

				string_append(&val, c);
			}
		}
		if (c == EOF) {
			j->error = "String without closing quote mark";
			string_free(&val);
			return NULL;
		}

		json_object *s = add_object(j, JSON_STRING);
		if (s != NULL) {
			s->string = val.buf;
			s->length = val.n;
		} else {
			string_free(&val);
		}
		return s;
	}

	j->error = "Found unexpected character";
	return NULL;
}

json_object *json_read(json_pull *j) {
	return json_read_separators(j, NULL, NULL);
}

json_object *json_read_tree(json_pull *p) {
	json_object *j;

	while ((j = json_read(p)) != NULL) {
		if (j->parent == NULL) {
			return j;
		}
	}

	return NULL;
}

void json_free(json_object *o) {
	size_t i;

	if (o == NULL) {
		return;
	}

	// Free any data linked from here

	if (o->type == JSON_ARRAY) {
		json_object **a = o->array;
		size_t n = o->length;

		o->array = NULL;
		o->length = 0;

		for (i = 0; i < n; i++) {
			json_free(a[i]);
		}

		free(a);
	} else if (o->type == JSON_HASH) {
		json_object **k = o->keys;
		json_object **v = o->values;
		size_t n = o->length;

		o->keys = NULL;
		o->values = NULL;
		o->length = 0;

		for (i = 0; i < n; i++) {
			json_free(k[i]);
			json_free(v[i]);
		}

		free(k);
		free(v);
	} else if (o->type == JSON_STRING || o->type == JSON_NUMBER) {
		free(o->string);
	}

	json_disconnect(o);

	free(o);
}

static void json_disconnect_parser(json_object *o) {
	if (o->type == JSON_HASH) {
		size_t i;
		for (i = 0; i < o->length; i++) {
			json_disconnect_parser(o->keys[i]);
			json_disconnect_parser(o->values[i]);
		}
	} else if (o->type == JSON_ARRAY) {
		size_t i;
		for (i = 0; i < o->length; i++) {
			json_disconnect_parser(o->array[i]);
		}
	}

	o->parser = NULL;
}

void json_disconnect(json_object *o) {
	// Expunge references to this as an array element
	// or a hash key or value.

	if (o->parent != NULL) {
		if (o->parent->type == JSON_ARRAY) {
			size_t i;

			for (i = 0; i < o->parent->length; i++) {
				if (o->parent->array[i] == o) {
					break;
				}
			}

			if (i < o->parent->length) {
				memmove(o->parent->array + i, o->parent->array + i + 1, o->parent->length - i - 1);
				o->parent->length--;
			}
		}

		if (o->parent->type == JSON_HASH) {
			size_t i;

			for (i = 0; i < o->parent->length; i++) {
				if (o->parent->keys[i] == o) {
					o->parent->keys[i] = fabricate_object(o->parser, o->parent, JSON_NULL);
					break;
				}
				if (o->parent->values[i] == o) {
					o->parent->values[i] = fabricate_object(o->parser, o->parent, JSON_NULL);
					break;
				}
			}

			if (i < o->parent->length) {
				if (o->parent->keys[i] != NULL && o->parent->keys[i]->type == JSON_NULL) {
					if (o->parent->values[i] != NULL && o->parent->values[i]->type == JSON_NULL) {
						free(o->parent->keys[i]);
						free(o->parent->values[i]);

						memmove(o->parent->keys + i, o->parent->keys + i + 1, o->parent->length - i - 1);
						memmove(o->parent->values + i, o->parent->values + i + 1, o->parent->length - i - 1);
						o->parent->length--;
					}
				}
			}
		}
	}

	if (o->parser != NULL && o->parser->root == o) {
		o->parser->root = NULL;
	}

	json_disconnect_parser(o);
	o->parent = NULL;
}

static void json_print_one(struct string *val, json_object *o) {
	if (o == NULL) {
		string_append_string(val, "...");
	} else if (o->type == JSON_STRING) {
		string_append(val, '\"');

		char *cp;
		for (cp = o->string; *cp != '\0'; cp++) {
			if (*cp == '\\' || *cp == '"') {
				string_append(val, '\\');
				string_append(val, *cp);
			} else if (*cp >= 0 && *cp < ' ') {
				char *s;
				if (asprintf(&s, "\\u%04x", *cp) >= 0) {
					string_append_string(val, s);
					free(s);
				}
			} else {
				string_append(val, *cp);
			}
		}

		string_append(val, '\"');
	} else if (o->type == JSON_NUMBER) {
		string_append_string(val, o->string);
	} else if (o->type == JSON_NULL) {
		string_append_string(val, "null");
	} else if (o->type == JSON_TRUE) {
		string_append_string(val, "true");
	} else if (o->type == JSON_FALSE) {
		string_append_string(val, "false");
	} else if (o->type == JSON_HASH) {
		string_append(val, '}');
	} else if (o->type == JSON_ARRAY) {
		string_append(val, ']');
	}
}

static void json_print(struct string *val, json_object *o) {
	if (o == NULL) {
		// Hash value in incompletely read hash
		string_append_string(val, "...");
	} else if (o->type == JSON_HASH) {
		string_append(val, '{');

		size_t i;
		for (i = 0; i < o->length; i++) {
			json_print(val, o->keys[i]);
			string_append(val, ':');
			json_print(val, o->values[i]);
			if (i + 1 < o->length) {
				string_append(val, ',');
			}
		}
		string_append(val, '}');
	} else if (o->type == JSON_ARRAY) {
		string_append(val, '[');
		size_t i;
		for (i = 0; i < o->length; i++) {
			json_print(val, o->array[i]);
			if (i + 1 < o->length) {
				string_append(val, ',');
			}
		}
		string_append(val, ']');
	} else {
		json_print_one(val, o);
	}
}

char *json_stringify(json_object *o) {
	struct string val;
	string_init(&val);
	json_print(&val, o);

	return val.buf;
}
