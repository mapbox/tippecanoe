#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <string>
#include <getopt.h>
#include <vector>
#include "jsonpull/jsonpull.h"

int fail = EXIT_SUCCESS;
bool wrap = false;
const char *extract = NULL;

FILE *csvfile = NULL;
std::vector<std::string> cols;
std::vector<std::string> fields;

std::string buffered;
int buffered_type = -1;
// 0: nothing yet
// 1: buffered a line
// 2: wrote the line and the wrapper
int buffer_state = 0;

std::vector<unsigned long> decode32(const char *s) {
	std::vector<unsigned long> utf32;

	while (*s != '\0') {
		unsigned long b = *(s++) & 0xFF;

		if (b < 0x80) {
			utf32.push_back(b);
		} else if ((b & 0xe0) == 0xc0) {
			unsigned long c = (b & 0x1f) << 6;
			unsigned long b1 = *(s++) & 0xFF;

			if ((b1 & 0xc0) == 0x80) {
				c |= b1 & 0x3f;
				utf32.push_back(c);
			} else {
				s--;
				utf32.push_back(0xfffd);
			}
		} else if ((b & 0xf0) == 0xe0) {
			unsigned long c = (b & 0x0f) << 12;
			unsigned long b1 = *(s++) & 0xFF;

			if ((b1 & 0xc0) == 0x80) {
				c |= (b1 & 0x3f) << 6;
				unsigned long b2 = *(s++) & 0xFF;

				if ((b2 & 0xc0) == 0x80) {
					c |= b2 & 0x3f;
					utf32.push_back(c);
				} else {
					s -= 2;
					utf32.push_back(0xfffd);
				}
			} else {
				s--;
				utf32.push_back(0xfffd);
			}
		} else if ((b & 0xf8) == 0xf0) {
			unsigned long c = (b & 0x07) << 18;
			unsigned long b1 = *(s++) & 0xFF;

			if ((b1 & 0xc0) == 0x80) {
				c |= (b1 & 0x3f) << 12;
				unsigned long b2 = *(s++) & 0xFF;

				if ((b2 & 0xc0) == 0x80) {
					c |= (b2 & 0x3f) << 6;
					unsigned long b3 = *(s++) & 0xFF;

					if ((b3 & 0xc0) == 0x80) {
						c |= b3 & 0x3f;

						utf32.push_back(c);
					} else {
						s -= 3;
						utf32.push_back(0xfffd);
					}
				} else {
					s -= 2;
					utf32.push_back(0xfffd);
				}
			} else {
				s -= 1;
				utf32.push_back(0xfffd);
			}
		} else {
			utf32.push_back(0xfffd);
		}
	}

	return utf32;
}

// This uses a really weird encoding for strings
// so that they will sort in UTF-32 order in spite of quoting

std::string sort_quote(const char *s) {
	std::vector<unsigned long> utf32 = decode32(s);
	std::string ret;

	for (size_t i = 0; i < utf32.size(); i++) {
		if (utf32[i] < 0xD800) {
			char buf[7];
			sprintf(buf, "\\u%04lu", utf32[i]);
			ret.append(std::string(buf));
		} else {
			unsigned long c = utf32[i];

			if (c <= 0x7f) {
				ret.push_back(c);
			} else if (c <= 0x7ff) {
				ret.push_back(0xc0 | (c >> 6));
				ret.push_back(0x80 | (c & 0x3f));
			} else if (c <= 0xffff) {
				ret.push_back(0xe0 | (c >> 12));
				ret.push_back(0x80 | ((c >> 6) & 0x3f));
				ret.push_back(0x80 | (c & 0x3f));
			} else {
				ret.push_back(0xf0 | (c >> 18));
				ret.push_back(0x80 | ((c >> 12) & 0x3f));
				ret.push_back(0x80 | ((c >> 6) & 0x3f));
				ret.push_back(0x80 | (c & 0x3f));
			}
		}
	}

	return ret;
}

void out(std::string const &s, int type, json_object *properties) {
	if (extract != NULL) {
		std::string extracted = sort_quote("null");

		json_object *o = json_hash_get(properties, extract);
		if (o != NULL) {
			if (o->type == JSON_STRING || o->type == JSON_NUMBER) {
				extracted = sort_quote(o->string);
			} else {
				// Don't really know what to do about sort quoting
				// for arbitrary objects

				const char *out = json_stringify(o);
				extracted = sort_quote(out);
				free((void *) out);
			}
		}

		printf("{\"%s\":%s}\n", extracted.c_str(), s.c_str());
		return;
	}

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
			out(s, 1, json_hash_get(j, "properties"));
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
				out(s, 2, NULL);
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
	const char *csv = NULL;

	struct option long_options[] = {
		{"wrap", no_argument, 0, 'w'},
		{"extract", required_argument, 0, 'e'},
		{"csv", required_argument, 0, 'c'},

		{0, 0, 0, 0},
	};

	std::string getopt_str;
	for (size_t lo = 0; long_options[lo].name != NULL; lo++) {
		if (long_options[lo].val > ' ') {
			getopt_str.push_back(long_options[lo].val);

			if (long_options[lo].has_arg == required_argument) {
				getopt_str.push_back(':');
			}
		}
	}

	extern int optind;
	int i;

	while ((i = getopt_long(argc, argv, getopt_str.c_str(), long_options, NULL)) != -1) {
		switch (i) {
		case 'w':
			wrap = true;
			break;

		case 'e':
			extract = optarg;
			break;

		case 'c':
			csv = optarg;
			break;

		default:
			fprintf(stderr, "Unexpected option -%c\n", i);
			exit(EXIT_FAILURE);
		}
	}

	if (extract != NULL && wrap) {
		fprintf(stderr, "%s: --wrap and --extract not supported together\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (csv != NULL) {
		csvfile = fopen(csv, "r");
		if (csvfile == NULL) {
			perror(csv);
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

	if (csvfile != NULL) {
		if (fclose(csvfile) != 0) {
			perror("close");
			exit(EXIT_FAILURE);
		}
	}

	return fail;
}
