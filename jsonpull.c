#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include "jsonpull.h"

#define BUFFER 10000

json_pull *json_begin(int (*read)(struct json_pull *, char *buffer, int n), void *source) {
	json_pull *j = malloc(sizeof(json_pull));

	j->error = NULL;
	j->line = 1;
	j->container = NULL;
	j->root = NULL;

	j->read = read;
	j->source = source;
	j->buffer = malloc(BUFFER);
	j->buffer_head = 0;
	j->buffer_tail = 0;

	return j;
}

static inline int peek(json_pull *j) {
	if (j->buffer_head < j->buffer_tail) {
		return j->buffer[j->buffer_head];
	} else {
		j->buffer_head = 0;
		j->buffer_tail = j->read(j, j->buffer, BUFFER);
		if (j->buffer_head >= j->buffer_tail) {
			return EOF;
		}
		return j->buffer[j->buffer_head];
	}
}

static inline int next(json_pull *j) {
	if (j->buffer_head < j->buffer_tail) {
		return j->buffer[j->buffer_head++];
	} else {
		j->buffer_head = 0;
		j->buffer_tail = j->read(j, j->buffer, BUFFER);
		if (j->buffer_head >= j->buffer_tail) {
			return EOF;
		}
		return j->buffer[j->buffer_head++];
	}
}

static int read_file(json_pull *j, char *buffer, int n) {
	return fread(buffer, 1, n, j->source);
}

json_pull *json_begin_file(FILE *f) {
	return json_begin(read_file, f);
}

#if 0
static int read_string(json_pull *j) {
	char *cp = j->source;
	if (*cp == '\0') {
		return EOF;
	}
	int c = (unsigned char) *cp;
	j->source = cp + 1;
	return c;
}

static int peek_string(json_pull *p) {
	char *cp = p->source;
	if (*cp == '\0') {
		return EOF;
	}
	return (unsigned char) *cp;
}

json_pull *json_begin_string(char *s) {
	return json_begin(read_string, peek_string, s);
}
#endif

void json_end(json_pull *p) {
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

#define SIZE_FOR(i) (((i) + 31) & ~31)

static json_object *fabricate_object(json_object *parent, json_type type) {
	json_object *o = malloc(sizeof(struct json_object));
	o->type = type;
	o->parent = parent;
	o->array = NULL;
	o->keys = NULL;
	o->values = NULL;
	o->length = 0;
	return o;
}

static json_object *add_object(json_pull *j, json_type type) {
	json_object *c = j->container;
	json_object *o = fabricate_object(c, type);

	if (c != NULL) {
		if (c->type == JSON_ARRAY) {
			if (c->expect == JSON_ITEM) {
				if (SIZE_FOR(c->length + 1) != SIZE_FOR(c->length)) {
					c->array = realloc(c->array, SIZE_FOR(c->length + 1) * sizeof(json_object *));
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

				if (SIZE_FOR(c->length + 1) != SIZE_FOR(c->length)) {
					c->keys = realloc(c->keys, SIZE_FOR(c->length + 1) * sizeof(json_object *));
					c->values = realloc(c->values, SIZE_FOR(c->length + 1) * sizeof(json_object *));
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
		j->root = o;
	}

	return o;
}

json_object *json_hash_get(json_object *o, const char *s) {
	if (o == NULL || o->type != JSON_HASH) {
		return NULL;
	}

	int i;
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
	int n;
	int nalloc;
};

static void string_init(struct string *s) {
	s->nalloc = 500;
	s->buf = malloc(s->nalloc);
	s->n = 0;
	s->buf[0] = '\0';
}

static void string_append(struct string *s, char c) {
	if (s->n + 2 >= s->nalloc) {
		s->nalloc += 500;
		s->buf = realloc(s->buf, s->nalloc);
	}

	s->buf[s->n++] = c;
	s->buf[s->n] = '\0';
}

static void string_free(struct string *s) {
	free(s->buf);
}

json_object *json_read_separators(json_pull *j, json_separator_callback cb, void *state) {
	int c;

	// In case there is an error at the top level
	if (j->container == NULL) {
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
	} while (c == ' ' || c == '\t' || c == '\r' || c == '\n');

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
			if (! (j->container->expect == JSON_ITEM && j->container->length == 0)) {
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
			if (! (j->container->expect == JSON_KEY && j->container->length == 0)) {
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
		if (j->container == NULL) {
			j->error = "Found comma at top level";
			return NULL;
		}

		if (j->container->expect != JSON_COMMA) {
			j->error = "Found unexpected comma";
			return NULL;
		}

		if (j->container->type == JSON_HASH) {
			j->container->expect = JSON_KEY;
		} else {
			j->container->expect = JSON_ITEM;
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

		while ((c = read_wrap(j)) != EOF) {
			if (c == '"') {
				break;
			} else if (c == '\\') {
				c = read_wrap(j);

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
				} else if (c == 'u') {
					char hex[5] = "aaaa";
					int i;
					for (i = 0; i < 4; i++) {
						hex[i] = read_wrap(j);
					}
					unsigned long ch = strtoul(hex, NULL, 16);
					if (ch <= 0x7F) {
						string_append(&val, ch);
					} else if (ch <= 0x7FF) {
						string_append(&val, 0xC0 | (ch >> 6));
						string_append(&val, 0x80 | (ch & 0x3F));
					} else {
						string_append(&val, 0xE0 | (ch >> 12));
						string_append(&val, 0x80 | ((ch >> 6) & 0x3F));
						string_append(&val, 0x80 | (ch & 0x3F));
					}
				} else {
					j->error = "Found backslash followed by unknown character";
					string_free(&val);
					return NULL;
				}
			} else {
				string_append(&val, c);
			}
		}

		json_object *s = add_object(j, JSON_STRING);
		if (s != NULL) {
			val.buf = realloc(val.buf, val.n + 1);
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
	int i;

	if (o == NULL) {
		return;
	}

	// Free any data linked from here

	if (o->type == JSON_ARRAY) {
		json_object **a = o->array;
		int n = o->length;

		o->array = NULL;
		o->length = 0;

		for (i = 0; i < n; i++) {
			json_free(a[i]);
		}

		free(a);
	} else if (o->type == JSON_HASH) {
		json_object **k = o->keys;
		json_object **v = o->values;
		int n = o->length;

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

	// Expunge references to this as an array element
	// or a hash key or value.

	if (o->parent != NULL) {
		if (o->parent->type == JSON_ARRAY) {
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
			for (i = 0; i < o->parent->length; i++) {
				if (o->parent->keys[i] == o) {
					o->parent->keys[i] = fabricate_object(o->parent, JSON_NULL);
					break;
				}
				if (o->parent->values[i] == o) {
					o->parent->values[i] = fabricate_object(o->parent, JSON_NULL);
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

	free(o);
}
