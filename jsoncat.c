#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include "jsonpull.h"

static void indent(int depth) {
	int i;
	for (i = 0; i < depth; i++) {
		printf("    ");
	}
}

void json_print(json_object *j, int depth) {
	if (j == NULL) {
		printf("NULL");
	} else if (j->type == JSON_STRING) {
		printf("\"");

		char *cp;
		for (cp = j->string; *cp != '\0'; cp++) {
			if (*cp == '\\' || *cp == '"') {
				printf("\\%c", *cp);
			} else if (*cp >= 0 && *cp < ' ') {
				printf("\\u%04x", *cp);
			} else {
				putchar(*cp);
			}
		}

		printf("\"");
	} else if (j->type == JSON_NUMBER) {
		printf("%f", j->number);
	} else if (j->type == JSON_NULL) {
		printf("null");
	} else if (j->type == JSON_TRUE) {
		printf("true");
	} else if (j->type == JSON_FALSE) {
		printf("false");
	} else if (j->type == JSON_HASH) {
		printf("{\n");
		indent(depth + 1);

		json_hash *h = j->hash;
		while (h != NULL) {
			json_print(h->key, depth + 1);
			printf(" : ");
			json_print(h->value, depth + 1);
			if (h->next != NULL) {
				printf(",\n");
				indent(depth + 1);
			}
			h = h->next;
		}
		printf("\n");
		indent(depth);
		printf("}");
	} else if (j->type == JSON_ARRAY) {
		printf("[\n");
		indent(depth + 1);

		json_array *a = j->array;
		while (a != NULL) {
			json_print(a->object, depth + 1);
			if (a->next != NULL) {
				printf(",\n");
				indent(depth + 1);
			}
			a = a->next;
		}
		printf("\n");
		indent(depth);
		printf("]");
	} else {
		printf("what type? %d", j->type);
	}
}

void process(FILE *f) {
	json_object *j = NULL;

	while ((j = json_parse(f, j)) != NULL) {
		if (j->parent == NULL) {
			json_print(j, 0);
			printf("\n");
			j = NULL;
		}
	}
}

int main(int argc, char **argv) {
	if (argc == 1) {
		process(stdin);
	} else {
		int i;
		for (i = 1; i < argc; i++) {
			FILE *f = fopen(argv[i], "r");
			if (f == NULL) {
				perror(argv[i]);
				exit(EXIT_FAILURE);
			}
			process(f);
			fclose(f);
		}
	}
}
