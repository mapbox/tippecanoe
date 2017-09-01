#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <string>
#include "jsonpull/jsonpull.h"

int fail = EXIT_SUCCESS;
bool wrap = false;

std::string buffered;
int buffered_type = -1;
// 0: nothing yet
// 1: buffered a line
// 2: wrote the line and the wrapper
int buffer_state = 0;

void out(std::string s, int type) {
	if (!wrap) {
		printf("%s\n", s.c_str());
		return;
	}

	if (buffer_state == 0) {
		buffered = s;
		buffered_type = type;
		buffer_state = 1;
		return;
	}

	if (buffer_state == 1) {
		if (buffered_type == 1) {
			printf("{\"type\":\"FeatureCollection\",\"features\":[\n");
		} else {
			printf("{\"type\":\"GeometryCollection\",\"geometries\":[\n");
		}

		printf("%s\n", buffered.c_str());
		buffer_state = 2;
	}

	printf(",\n%s\n", s.c_str());

	if (type != buffered_type) {
		fprintf(stderr, "Error: mix of bare geometries and features\n");
		exit(EXIT_FAILURE);
	}
}

void process(FILE *fp, const char *fname) {
	json_pull *jp = json_begin_file(fp);

	while (1) {
		json_object *j = json_read(jp);
		if (j == NULL) {
			if (jp->error != NULL) {
				fprintf(stderr, "%s:%d: %s\n", fname, jp->line, jp->error);
			}

			json_free(jp->root);
			break;
		}

		json_object *type = json_hash_get(j, "type");
		if (type == NULL || type->type != JSON_STRING) {
			continue;
		}

		if (strcmp(type->string, "Feature") == 0) {
			char *s = json_stringify(j);
			out(s, 1);
			free(s);
			json_free(j);
		} else if (strcmp(type->string, "Point") == 0 ||
			   strcmp(type->string, "MultiPoint") == 0 ||
			   strcmp(type->string, "LineString") == 0 ||
			   strcmp(type->string, "MultiLineString") == 0 ||
			   strcmp(type->string, "MultiPolygon") == 0) {
			int is_geometry = 1;

			if (j->parent != NULL) {
				if (j->parent->type == JSON_ARRAY) {
					if (j->parent->parent->type == JSON_HASH) {
						json_object *geometries = json_hash_get(j->parent->parent, "geometries");
						if (geometries != NULL) {
							// Parent of Parent must be a GeometryCollection
							is_geometry = 0;
						}
					}
				} else if (j->parent->type == JSON_HASH) {
					json_object *geometry = json_hash_get(j->parent, "geometry");
					if (geometry != NULL) {
						// Parent must be a Feature
						is_geometry = 0;
					}
				}
			}

			if (is_geometry) {
				char *s = json_stringify(j);
				out(s, 2);
				free(s);
				json_free(j);
			}
		} else if (strcmp(type->string, "FeatureCollection") == 0) {
			json_free(j);
		}
	}

	json_end(jp);
}

int main(int argc, char **argv) {
	extern int optind;
	int i;

	while ((i = getopt(argc, argv, "w")) != -1) {
		switch (i) {
		case 'w':
			wrap = true;
			break;

		default:
			fprintf(stderr, "Unexpected option -%c\n", i);
			exit(EXIT_FAILURE);
		}
	}

	if (optind >= argc) {
		process(stdin, "standard input");
	} else {
		for (i = optind; i < argc; i++) {
			FILE *f = fopen(argv[i], "r");
			if (f == NULL) {
				perror(argv[i]);
				exit(EXIT_FAILURE);
			}

			process(f, argv[i]);
			fclose(f);
		}
	}

	if (buffer_state == 1) {
		printf("%s\n", buffered.c_str());
	} else if (buffer_state == 2) {
		printf("]}\n");
	}

	return fail;
}
