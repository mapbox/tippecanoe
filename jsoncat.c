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
		printf("\"%s\"", j->string);
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

int main() {
	json_object *j = NULL;

	while ((j = json_parse(stdin, j)) != NULL) {
		json_object *g = json_hash_get(j, "type");
		if (g != NULL && g->type == JSON_STRING && strcmp(g->string, "Feature") == 0) {
			json_print(j, 0);
			printf("\n");
		}

		if (j->parent == NULL) {
			json_print(j, 0);
			printf("\n");
			j = NULL;
		}
	}
}
