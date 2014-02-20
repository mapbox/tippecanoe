#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include "jsonpull.h"

static void indent(int depth) {
	int i;
	for (i = 0; i < depth; i++) {
		printf("    ");
	}
}

void json_print_one(json_object *j, int *depth) {
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
		printf("\n");
		(*depth)--;
		indent(*depth);
		printf("}");
	} else if (j->type == JSON_ARRAY) {
		printf("\n");
		(*depth)--;
		indent(*depth);
		printf("]");
	}
}

void json_print(json_object *j, int depth) {
	if (j == NULL) {
		// Hash value in incompletely read hash
		printf("NULL");
	} else if (j->type == JSON_HASH) {
		printf("{\n");
		indent(depth + 1);

		int i;
		for (i = 0; i < j->length; i++) {
			json_print(j->keys[i], depth + 1);
			printf(" : ");
			json_print(j->values[i], depth + 1);
			if (i + 1 < j->length) {
				printf(",\n");
				indent(depth + 1);
			}
		}
		printf("\n");
		indent(depth);
		printf("}");
	} else if (j->type == JSON_ARRAY) {
		printf("[\n");
		indent(depth + 1);

		int i;
		for (i = 0; i < j->length; i++) {
			json_print(j->array[i], depth + 1);
			if (i + 1 < j->length) {
				printf(",\n");
				indent(depth + 1);
			}
		}
		printf("\n");
		indent(depth);
		printf("]");
	} else {
		json_print_one(j, &depth);
	}
}

void callback(json_type type, json_pull *jp, void *state) {
	int *level = state;

	if (type == JSON_ARRAY) {
		printf("[\n");
		(*level)++;
		indent(*level);
	} else if (type == JSON_HASH) {
		printf("{\n");
		(*level)++;
		indent(*level);
	} else if (type == JSON_COMMA) {
		printf(",\n");
		indent(*level);
	} else if (type == JSON_COLON) {
		printf(" : ");
	}
}

void process_callback(FILE *f, char *fname) {
	json_pull *jp = json_begin_file(f);
	json_object *j;
	int level = 0;

	while ((j = json_read_separators(jp, callback, &level)) != NULL) {
		json_print_one(j, &level);
		json_free(j);

		if (j->parent == NULL) {
			printf("\n");
		}
	}

	if (jp->error != NULL) {
		fflush(stdout);
		fprintf(stderr, "%s: %d: %s\n", fname, jp->line, jp->error);
		json_free(jp->root);
	}

	json_end(jp);
}

void process_incremental(FILE *f, char *fname) {
	json_pull *jp = json_begin_file(f);
	json_object *j;

	while ((j = json_read(jp)) != NULL) {
		if (j->parent == NULL) {
			json_print(j, 0);
			json_free(j);
			printf("\n");
		}
	}

	if (jp->error != NULL) {
		fflush(stdout);
		fprintf(stderr, "%s: %d: %s\n", fname, jp->line, jp->error);
		json_free(jp->root);
	}

	json_end(jp);
}

void process_tree(FILE *f, char *fname) {
	json_pull *jp = json_begin_file(f);

	while (1) {
		json_object *j = json_read_tree(jp);

		if (j == NULL) {
			if (jp->error != NULL) {
				fprintf(stderr, "%s: %d: %s\n", fname, jp->line, jp->error);
				json_free(jp->root);
			}

			break;
		}

		json_print(j, 0);
		json_free(j);
		printf("\n");
	}

	json_end(jp);
}

int main(int argc, char **argv) {
	extern int optind;
	int i;

	void (*func)(FILE *, char *) = process_callback;

	while ((i = getopt(argc, argv, "ti")) != -1) {
		switch (i) {
		case 't':
			func = process_tree;
			break;

		case 'i':
			func = process_incremental;
			break;

		default:
			fprintf(stderr, "Unexpected option -%c\n", i);
			exit(EXIT_FAILURE);
		}
	}

	if (optind >= argc) {
		func(stdin, "standard input");
	} else {
		int i;
		for (i = optind; i < argc; i++) {
			FILE *f = fopen(argv[i], "r");
			if (f == NULL) {
				perror(argv[i]);
				exit(EXIT_FAILURE);
			}
			func(f, argv[i]);
			fclose(f);
		}
	}

	return 0;
}
