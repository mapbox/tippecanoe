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
	json_hash *object;
} json_object;

json_object *new_object(json_type type, json_object *parent) {
	json_object *o = malloc(sizeof(struct json_object));
	o->type = type;
	o->parent = parent;
	return o;
}

static void json_error(char *s) {
	fprintf(stderr, "%s\n", s);
	exit(EXIT_FAILURE);
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
		return parse(f, new_object(JSON_ARRAY, current));
	} else if (c == ']') {
		if (current->parent == NULL || current->parent->type != JSON_ARRAY) {
			json_error("] without [");
		}

		return current->parent;
	}

	if (c == '{') {
		return parse(f, new_object(JSON_HASH, current));
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

		return new_object(JSON_NULL, current);
	}

	if (c == 't') {
		if (getc(f) != 'r' || getc(f) != 'u' || getc(f) != 'e') {
			json_error("misspelled true");
		}

		return new_object(JSON_TRUE, current);
	}

	if (c == 'f') {
		if (getc(f) != 'a' || getc(f) != 'l' || getc(f) != 's' || getc(f) != 'e') {
			json_error("misspelled false");
		}

		return new_object(JSON_FALSE, current);
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

		return parse(f, current);
	}
}
