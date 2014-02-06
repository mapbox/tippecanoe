#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

typedef enum json_type {
	JSON_STRING, JSON_NUMBER, JSON_ARRAY, JSON_HASH, JSON_NULL, JSON_TRUE, JSON_FALSE,
} json_type;

typedef struct json_array {
	struct json_object *object;
	struct json_array *next;
} json_array;

typedef struct json_hash {
	struct json_object *key;
	struct json_object *value;
	struct json_hash *next;
} json_hash;

typedef struct json_object {
	json_type type;
	struct json_object *parent;

	char *string;
	double number;
	json_array *array;
	json_hash *hash;
} json_object;

static json_object *add_object(json_type type, json_object *parent) {
	json_object *o = malloc(sizeof(struct json_object));
	o->type = type;
	o->parent = parent;
	o->array = NULL;
	o->hash = NULL;

	if (parent != NULL) {
		if (parent->type == JSON_ARRAY) {
			json_array *a = malloc(sizeof(json_array));
			a->next = parent->array;
			a->object = o;
			parent->array = a;
		}

		if (parent->type == JSON_HASH) {
			if (parent->hash != NULL && parent->hash->value == NULL) {
				parent->hash->value = o;
			} else {
				json_hash *h = malloc(sizeof(json_hash));
				h->next = parent->hash;
				h->key = o;
				parent->hash = h;
			}
		}
	}

	return o;
}

static void json_error(char *s) {
	fprintf(stderr, "%s\n", s);
	exit(EXIT_FAILURE);
}

int peek(FILE *f) {
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

json_object *parse(FILE *f, json_object *current) {
	int c = getc(f);
	if (c == EOF) {
		return NULL;
	}

	while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
		c = getc(f);
		if (c == EOF) {
			return NULL;
		}
	}

	if (c == '[') {
		return parse(f, add_object(JSON_ARRAY, current));
	} else if (c == ']') {
		if (current->parent == NULL || current->parent->type != JSON_ARRAY) {
			json_error("] without [");
		}

		return current->parent;
	}

	if (c == '{') {
		return parse(f, add_object(JSON_HASH, current));
	} else if (c == '}') {
		if (current->parent == NULL || current->parent->type != JSON_HASH) {
			json_error("} without {");
		}

		return current->parent;
	}

	if (c == 'n') {
		if (getc(f) != 'u' || getc(f) != 'l' || getc(f) != 'l') {
			json_error("misspelled null");
		}

		return add_object(JSON_NULL, current);
	}

	if (c == 't') {
		if (getc(f) != 'r' || getc(f) != 'u' || getc(f) != 'e') {
			json_error("misspelled true");
		}

		return add_object(JSON_TRUE, current);
	}

	if (c == 'f') {
		if (getc(f) != 'a' || getc(f) != 'l' || getc(f) != 's' || getc(f) != 'e') {
			json_error("misspelled false");
		}

		return add_object(JSON_FALSE, current);
	}

	if (c == ',') {
		if (current->parent == NULL ||
			(current->parent->type != JSON_ARRAY ||
			 current->parent->type != JSON_HASH)) {
			json_error(", not in array or hash");
		}

		return parse(f, current);
	}

	if (c == ':') {
		if (current->parent == NULL || current->parent->type != JSON_HASH) {
			json_error(": not in hash");
		}
		if (current->parent->hash == NULL || current->parent->hash->value != NULL) {
			json_error(": without key");
		}

		return parse(f, current);
	}

	if (c == '-' || (c >= '0' && c <= '9')) {
		struct string val;
		string_init(&val);

		string_append(&val, c);

		if (c >= '1' && c <= '9') {
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
}
