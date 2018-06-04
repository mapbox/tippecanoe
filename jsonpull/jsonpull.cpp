#define _GNU_SOURCE  // for asprintf()
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include "jsonpull.hpp"

#define BUFFER 10000

json_pull *json_begin(ssize_t (*read)(struct json_pull *, char *buffer, size_t n), void *source) {
	json_pull *j = new json_pull;
	if (j == NULL) {
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}

	j->error = "";
	j->line = 1;
	j->container = NULL;
	j->root = NULL;

	j->read = read;
	j->source = source;
	j->buffer_head = 0;
	j->buffer_tail = 0;

	j->buffer.resize(BUFFER);

	return j;
}

static inline int peek(json_pull *j) {
	if (j->buffer_head < j->buffer_tail) {
		return (unsigned char) j->buffer[j->buffer_head];
	} else {
		j->buffer_head = 0;
		j->buffer_tail = j->read(j, (char *) j->buffer.c_str(), BUFFER);
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
		j->buffer_tail = j->read(j, (char *) j->buffer.c_str(), BUFFER);
		if (j->buffer_head >= j->buffer_tail) {
			return EOF;
		}
		return (unsigned char) j->buffer[j->buffer_head++];
	}
}

static ssize_t read_file(json_pull *j, char *buffer, size_t n) {
	return fread(buffer, 1, n, (FILE *) j->source);
}

json_pull *json_begin_file(FILE *f) {
	return json_begin(read_file, f);
}

static ssize_t read_string(json_pull *j, char *buffer, size_t n) {
	const char *cp = (const char *) j->source;
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
	delete p;
}

static inline int read_wrap(json_pull *j) {
	int c = next(j);

	if (c == '\n') {
		j->line++;
	}

	return c;
}

static json_object *fabricate_object(json_pull *jp, json_object *parent, json_type type) {
	json_object *o = new json_object;
	if (o == NULL) {
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}
	o->type = type;
	o->parent = parent;
	o->parser = jp;
	return o;
}

static json_object *add_object(json_pull *j, json_type type) {
	json_object *c = j->container;
	json_object *o = fabricate_object(j, c, type);

	if (c != NULL) {
		if (c->type == JSON_ARRAY) {
			if (c->expect == JSON_ITEM) {
				c->array.push_back(o);
				c->expect = JSON_COMMA;
			} else {
				j->error = "Expected a comma, not a list item";
				delete o;
				return NULL;
			}
		} else if (c->type == JSON_HASH) {
			if (c->expect == JSON_VALUE) {
				c->values[c->values.size() - 1] = o;
				c->expect = JSON_COMMA;
			} else if (c->expect == JSON_KEY) {
				if (type != JSON_STRING) {
					j->error = "Hash key is not a string";
					delete o;
					return NULL;
				}

				c->keys.push_back(o);
				c->values.push_back(NULL);
				c->expect = JSON_COLON;
			} else {
				j->error = "Expected a comma or colon";
				delete o;
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

json_object *json_hash_get(json_object *o, std::string const &s) {
	if (o == NULL || o->type != JSON_HASH) {
		return NULL;
	}

	size_t i;
	for (i = 0; i < o->keys.size(); i++) {
		if (o->keys[i] != NULL && o->keys[i]->type == JSON_STRING) {
			if (o->keys[i]->string == s) {
				return o->values[i];
			}
		}
	}

	return NULL;
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
			if (!(j->container->expect == JSON_ITEM && j->container->array.size() == 0)) {
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
			if (!(j->container->expect == JSON_KEY && j->container->keys.size() == 0)) {
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
		std::string val;

		if (c == '-') {
			val.push_back(c);
			c = read_wrap(j);
		}

		if (c == '0') {
			val.push_back(c);
		} else if (c >= '1' && c <= '9') {
			val.push_back(c);
			c = peek(j);

			while (c >= '0' && c <= '9') {
				val.push_back(read_wrap(j));
				c = peek(j);
			}
		}

		if (peek(j) == '.') {
			val.push_back(read_wrap(j));

			c = peek(j);
			if (c < '0' || c > '9') {
				j->error = "Decimal point without digits";
				return NULL;
			}
			while (c >= '0' && c <= '9') {
				val.push_back(read_wrap(j));
				c = peek(j);
			}
		}

		c = peek(j);
		if (c == 'e' || c == 'E') {
			val.push_back(read_wrap(j));

			c = peek(j);
			if (c == '+' || c == '-') {
				val.push_back(read_wrap(j));
			}

			c = peek(j);
			if (c < '0' || c > '9') {
				j->error = "Exponent without digits";
				return NULL;
			}
			while (c >= '0' && c <= '9') {
				val.push_back(read_wrap(j));
				c = peek(j);
			}
		}

		json_object *n = add_object(j, JSON_NUMBER);
		if (n != NULL) {
			n->number = atof(val.c_str());
			n->string = val;
		}
		return n;
	}

	/////////////////////////// Strings

	if (c == '"') {
		std::string val;

		int surrogate = -1;
		while ((c = read_wrap(j)) != EOF) {
			if (c == '"') {
				if (surrogate >= 0) {
					val.push_back(0xE0 | (surrogate >> 12));
					val.push_back(0x80 | ((surrogate >> 6) & 0x3F));
					val.push_back(0x80 | (surrogate & 0x3F));
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
							val.push_back(0xE0 | (surrogate >> 12));
							val.push_back(0x80 | ((surrogate >> 6) & 0x3F));
							val.push_back(0x80 | (surrogate & 0x3F));
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
						val.push_back(0xE0 | (surrogate >> 12));
						val.push_back(0x80 | ((surrogate >> 6) & 0x3F));
						val.push_back(0x80 | (surrogate & 0x3F));
						surrogate = -1;
					}

					if (ch <= 0x7F) {
						val.push_back(ch);
					} else if (ch <= 0x7FF) {
						val.push_back(0xC0 | (ch >> 6));
						val.push_back(0x80 | (ch & 0x3F));
					} else if (ch < 0xFFFF) {
						val.push_back(0xE0 | (ch >> 12));
						val.push_back(0x80 | ((ch >> 6) & 0x3F));
						val.push_back(0x80 | (ch & 0x3F));
					} else {
						val.push_back(0xF0 | (ch >> 18));
						val.push_back(0x80 | ((ch >> 12) & 0x3F));
						val.push_back(0x80 | ((ch >> 6) & 0x3F));
						val.push_back(0x80 | (ch & 0x3F));
					}
				} else {
					if (surrogate >= 0) {
						val.push_back(0xE0 | (surrogate >> 12));
						val.push_back(0x80 | ((surrogate >> 6) & 0x3F));
						val.push_back(0x80 | (surrogate & 0x3F));
						surrogate = -1;
					}

					if (c == '"') {
						val.push_back('"');
					} else if (c == '\\') {
						val.push_back('\\');
					} else if (c == '/') {
						val.push_back('/');
					} else if (c == 'b') {
						val.push_back('\b');
					} else if (c == 'f') {
						val.push_back('\f');
					} else if (c == 'n') {
						val.push_back('\n');
					} else if (c == 'r') {
						val.push_back('\r');
					} else if (c == 't') {
						val.push_back('\t');
					} else {
						j->error = "Found backslash followed by unknown character";
						return NULL;
					}
				}
			} else if (c < ' ') {
				j->error = "Found control character in string";
				return NULL;
			} else {
				if (surrogate >= 0) {
					val.push_back(0xE0 | (surrogate >> 12));
					val.push_back(0x80 | ((surrogate >> 6) & 0x3F));
					val.push_back(0x80 | (surrogate & 0x3F));
					surrogate = -1;
				}

				val.push_back(c);
			}
		}
		if (c == EOF) {
			j->error = "String without closing quote mark";
			return NULL;
		}

		json_object *s = add_object(j, JSON_STRING);
		if (s != NULL) {
			s->string = val;
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
		std::vector<json_object *> a = o->array;
		size_t n = o->array.size();

		o->array.resize(0);

		for (i = 0; i < n; i++) {
			json_free(a[i]);
		}
	} else if (o->type == JSON_HASH) {
		std::vector<json_object *> k = o->keys;
		std::vector<json_object *> v = o->values;
		size_t n = o->keys.size();

		o->keys.resize(0);
		o->values.resize(0);

		for (i = 0; i < n; i++) {
			json_free(k[i]);
			json_free(v[i]);
		}
	}

	json_disconnect(o);

	delete o;
}

static void json_disconnect_parser(json_object *o) {
	if (o->type == JSON_HASH) {
		size_t i;
		for (i = 0; i < o->keys.size(); i++) {
			json_disconnect_parser(o->keys[i]);
			json_disconnect_parser(o->values[i]);
		}
	} else if (o->type == JSON_ARRAY) {
		size_t i;
		for (i = 0; i < o->array.size(); i++) {
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

			for (i = 0; i < o->parent->array.size(); i++) {
				if (o->parent->array[i] == o) {
					break;
				}
			}

			if (i < o->parent->array.size()) {
				o->parent->array.erase(o->parent->array.begin() + i);
			}
		}

		if (o->parent->type == JSON_HASH) {
			size_t i;

			for (i = 0; i < o->parent->keys.size(); i++) {
				if (o->parent->keys[i] == o) {
					o->parent->keys[i] = fabricate_object(o->parser, o->parent, JSON_NULL);
					break;
				}
				if (o->parent->values[i] == o) {
					o->parent->values[i] = fabricate_object(o->parser, o->parent, JSON_NULL);
					break;
				}
			}

			if (i < o->parent->keys.size()) {
				if (o->parent->keys[i] != NULL && o->parent->keys[i]->type == JSON_NULL) {
					if (o->parent->values[i] != NULL && o->parent->values[i]->type == JSON_NULL) {
						delete o->parent->keys[i];
						delete o->parent->values[i];

						o->parent->keys.erase(o->parent->keys.begin() + i);
						o->parent->values.erase(o->parent->values.begin() + i);
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

static void json_print_one(std::string &val, json_object *o) {
	if (o == NULL) {
		val.append("...");
	} else if (o->type == JSON_STRING) {
		val.push_back('\"');

		const char *cp;
		for (cp = o->string.c_str(); *cp != '\0'; cp++) {
			if (*cp == '\\' || *cp == '"') {
				val.push_back('\\');
				val.push_back(*cp);
			} else if (*cp >= 0 && *cp < ' ') {
				char *s;
				if (asprintf(&s, "\\u%04x", *cp) >= 0) {
					val.append(s);
					free(s);
				}
			} else {
				val.push_back(*cp);
			}
		}

		val.push_back('\"');
	} else if (o->type == JSON_NUMBER) {
		val.append(o->string);
	} else if (o->type == JSON_NULL) {
		val.append("null");
	} else if (o->type == JSON_TRUE) {
		val.append("true");
	} else if (o->type == JSON_FALSE) {
		val.append("false");
	} else if (o->type == JSON_HASH) {
		val.push_back('}');
	} else if (o->type == JSON_ARRAY) {
		val.push_back(']');
	}
}

static void json_print(std::string &val, json_object *o) {
	if (o == NULL) {
		// Hash value in incompletely read hash
		val.append("...");
	} else if (o->type == JSON_HASH) {
		val.push_back('{');

		size_t i;
		for (i = 0; i < o->keys.size(); i++) {
			json_print(val, o->keys[i]);
			val.push_back(':');
			json_print(val, o->values[i]);
			if (i + 1 < o->keys.size()) {
				val.push_back(',');
			}
		}
		val.push_back('}');
	} else if (o->type == JSON_ARRAY) {
		val.push_back('[');
		size_t i;
		for (i = 0; i < o->array.size(); i++) {
			json_print(val, o->array[i]);
			if (i + 1 < o->array.size()) {
				val.push_back(',');
			}
		}
		val.push_back(']');
	} else {
		json_print_one(val, o);
	}
}

std::string json_stringify(json_object *o) {
	std::string val;
	json_print(val, o);
	return val;
}
