#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include "jsonpull.h"

static void json_error(char *s, ...) {
	va_list ap;
	va_start(ap, s);
	vfprintf(stderr, s, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

static json_object *add_object(json_type type, json_object *parent) {
	json_object *o = malloc(sizeof(struct json_object));
	o->type = type;
	o->parent = parent;
	o->array = NULL;
	o->keys = NULL;
	o->values = NULL;
	o->length = 0;
	o->__allocated = 0;

	if (parent != NULL) {
		if (parent->type == JSON_ARRAY) {
			if (parent->length >= parent->__allocated) {
				parent->__allocated += 20;
				parent->array = realloc(parent->array, parent->__allocated * sizeof(json_object *));
			}

			parent->array[parent->length++] = o;
		}

		if (parent->type == JSON_HASH) {
			if (parent->length > 0 && parent->values[parent->length - 1] == NULL) {
				// Hash has key but no value, so this is the value
				parent->values[parent->length - 1] = o;
			} else {
				// No current hash, so this is a key

				if (type != JSON_STRING) {
					json_error("Hash key is not a string");
				}

				if (parent->length >= parent->__allocated) {
					parent->__allocated += 20;
					parent->keys = realloc(parent->keys, parent->__allocated * sizeof(json_object *));
					parent->values = realloc(parent->values, parent->__allocated * sizeof(json_object *));
				}

				parent->keys[parent->length] = o;
				parent->values[parent->length] = NULL;
				parent->length++;
			}
		}
	}

	return o;
}

json_object *json_hash_get(json_object *o, char *s) {
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

static int peek(FILE *f) {
	int c = getc(f);
	ungetc(c, f);
	return c;
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

json_object *json_parse(FILE *f, json_object *current) {
	int c = getc(f);
	if (c == EOF) {
		return NULL;
	}

	/////////////////////////// Whitespace

	while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
		c = getc(f);
		if (c == EOF) {
			return NULL;
		}
	}

	/////////////////////////// Arrays

	if (c == '[') {
		return json_parse(f, add_object(JSON_ARRAY, current));
	} else if (c == ']') {
		if (current == NULL) {
			json_error("] at top level");
		}

		if (current->type == JSON_ARRAY && current->length == 0) {
			return current; // Empty array
		}

		if (current->parent == NULL || current->parent->type != JSON_ARRAY) {
			json_error("] without [\n");
		}

		return current->parent;
	}

	/////////////////////////// Hashes

	if (c == '{') {
		return json_parse(f, add_object(JSON_HASH, current));
	} else if (c == '}') {
		if (current == NULL) {
			json_error("} at top level");
		}

		if (current->type == JSON_HASH && current->length == 0) {
			return current; // Empty hash
		}

		if (current->parent == NULL || current->parent->type != JSON_HASH) {
			json_error("} without {\n");
		}
		if (current->parent->length != 0 && current->parent->values[current->parent->length - 1] == NULL) {
			json_error("} without hash value\n");
		}

		return current->parent;
	}

	/////////////////////////// Null

	if (c == 'n') {
		if (getc(f) != 'u' || getc(f) != 'l' || getc(f) != 'l') {
			json_error("misspelled null\n");
		}

		return add_object(JSON_NULL, current);
	}

	/////////////////////////// True

	if (c == 't') {
		if (getc(f) != 'r' || getc(f) != 'u' || getc(f) != 'e') {
			json_error("misspelled true\n");
		}

		return add_object(JSON_TRUE, current);
	}

	/////////////////////////// False

	if (c == 'f') {
		if (getc(f) != 'a' || getc(f) != 'l' || getc(f) != 's' || getc(f) != 'e') {
			json_error("misspelled false\n");
		}

		return add_object(JSON_FALSE, current);
	}

	/////////////////////////// Comma

	if (c == ',') {
		if (current == NULL) {
			json_error("comma at top level");
		}

		if (current->parent == NULL ||
			(current->parent->type != JSON_ARRAY &&
			 current->parent->type != JSON_HASH)) {
			json_error(", not in array or hash\n");
		}

		return json_parse(f, current->parent);
	}

	/////////////////////////// Colon

	if (c == ':') {
		if (current == NULL) {
			json_error("colon at top level");
		}

		if (current->parent == NULL || current->parent->type != JSON_HASH) {
			json_error(": not in hash\n");
		}
		if (current->parent->length == 0 || current->parent->keys[current->parent->length - 1] == NULL) {
			json_error(": without key\n");
		}

		return json_parse(f, current->parent);
	}

	/////////////////////////// Numbers

	if (c == '-' || (c >= '0' && c <= '9')) {
		struct string val;
		string_init(&val);

		if (c == '-') {
			string_append(&val, c);
			c = getc(f);
		}

		if (c == '0') {
			string_append(&val, c);
		} else if (c >= '1' && c <= '9') {
			string_append(&val, c);
			c = peek(f);

			while (c >= '0' && c <= '9') {
				string_append(&val, getc(f));
				c = peek(f);
			}
		}

		if (peek(f) == '.') {
			string_append(&val, getc(f));

			c = peek(f);
			while (c >= '0' && c <= '9') {
				string_append(&val, getc(f));
				c = peek(f);
			}
		}

		c = peek(f);
		if (c == 'e' || c == 'E') {
			string_append(&val, getc(f));

			c = peek(f);
			if (c == '+' || c == '-') {
				string_append(&val, getc(f));
			}

			c = peek(f);
			while (c >= '0' && c <= '9') {
				string_append(&val, getc(f));
				c = peek(f);
			}
		}

		json_object *n = add_object(JSON_NUMBER, current);
		n->number = atof(val.buf);
		string_free(&val);

		return n;
	}

	/////////////////////////// Strings

	if (c == '"') {
		struct string val;
		string_init(&val);

		while ((c = getc(f)) != EOF) {
			if (c == '"') {
				break;
			} else if (c == '\\') {
				c = getc(f);

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
						hex[i] = getc(f);
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
					json_error("unknown string escape %c\n", c);
				}
			} else {
				string_append(&val, c);
			}
		}

		json_object *s = add_object(JSON_STRING, current);
		s->string = val.buf;

		return s;
	}

	json_error("unrecognized character %c\n", c);
	return NULL;
}
