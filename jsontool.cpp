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
#include "csv.hpp"
#include "text.hpp"
#include "geojson-loop.hpp"

int fail = EXIT_SUCCESS;
bool wrap = false;
const char *extract = NULL;

FILE *csvfile = NULL;
std::vector<std::string> header;
std::vector<std::string> fields;
int pe = false;

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
		bool found = false;

		json_object *o = json_hash_get(properties, extract);
		if (o != NULL) {
			found = true;
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

		if (!found) {
			static bool warned = false;
			if (!warned) {
				fprintf(stderr, "Warning: extract key \"%s\" not found in JSON\n", extract);
				warned = true;
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

std::string prev_joinkey;

void join_csv(json_object *j) {
	if (header.size() == 0) {
		std::string s = csv_getline(csvfile);
		if (s.size() == 0) {
			fprintf(stderr, "Couldn't get column header from CSV file\n");
			exit(EXIT_FAILURE);
		}

		std::string err = check_utf8(s);
		if (err != "") {
			fprintf(stderr, "%s\n", err.c_str());
			exit(EXIT_FAILURE);
		}

		header = csv_split(s.c_str());

		for (size_t i = 0; i < header.size(); i++) {
			header[i] = csv_dequote(header[i]);
		}

		if (header.size() == 0) {
			fprintf(stderr, "No columns in CSV header \"%s\"\n", s.c_str());
			exit(EXIT_FAILURE);
		}
	}

	json_object *properties = json_hash_get(j, "properties");
	json_object *key = NULL;

	if (properties != NULL) {
		key = json_hash_get(properties, header[0].c_str());
	}

	if (key == NULL) {
		static bool warned = false;
		if (!warned) {
			fprintf(stderr, "Warning: couldn't find CSV key \"%s\" in JSON\n", header[0].c_str());
			warned = true;
		}
		return;
	}

	std::string joinkey;
	if (key->type == JSON_STRING || key->type == JSON_NUMBER) {
		joinkey = key->string;
	} else {
		const char *s = json_stringify(key);
		joinkey = s;
		free((void *) s);
	}

	if (joinkey < prev_joinkey) {
		fprintf(stderr, "GeoJSON file is out of sort: \"%s\" follows \"%s\"\n", joinkey.c_str(), prev_joinkey.c_str());
		exit(EXIT_FAILURE);
	}
	prev_joinkey = joinkey;

	if (fields.size() == 0 || joinkey > fields[0]) {
		std::string prevkey;
		if (fields.size() > 0) {
			prevkey = fields[0];
		}

		while (true) {
			std::string s = csv_getline(csvfile);
			if (s.size() == 0) {
				fields.clear();
				break;
			}

			std::string err = check_utf8(s);
			if (err != "") {
				fprintf(stderr, "%s\n", err.c_str());
				exit(EXIT_FAILURE);
			}

			fields = csv_split(s.c_str());

			for (size_t i = 0; i < fields.size(); i++) {
				fields[i] = csv_dequote(fields[i]);
			}

			if (fields.size() > 0 && fields[0] < prevkey) {
				fprintf(stderr, "CSV file is out of sort: \"%s\" follows \"%s\"\n", fields[0].c_str(), prevkey.c_str());
				exit(EXIT_FAILURE);
			}

			if (fields.size() > 0 && fields[0] >= joinkey) {
				break;
			}

			if (fields.size() > 0) {
				prevkey = fields[0];
			}
		}
	}

	if (fields.size() > 0 && joinkey == fields[0]) {
		// This knows more about the structure of JSON objects than it ought to
		properties->keys = (json_object **) realloc((void *) properties->keys, (properties->length + 32 + fields.size()) * sizeof(json_object *));
		properties->values = (json_object **) realloc((void *) properties->values, (properties->length + 32 + fields.size()) * sizeof(json_object *));
		if (properties->keys == NULL || properties->values == NULL) {
			perror("realloc");
			exit(EXIT_FAILURE);
		}

		for (size_t i = 1; i < fields.size(); i++) {
			std::string k = header[i];
			std::string v = fields[i];
			json_type attr_type = JSON_STRING;

			if (v.size() > 0) {
				if (v[0] == '"') {
					v = csv_dequote(v);
				} else if (is_number(v)) {
					attr_type = JSON_NUMBER;
				}
			} else if (pe) {
				attr_type = JSON_NULL;
			}

			if (attr_type != JSON_NULL) {
				// This knows more about the structure of JSON objects than it ought to

				json_object *ko = (json_object *) malloc(sizeof(json_object));
				json_object *vo = (json_object *) malloc(sizeof(json_object));
				if (ko == NULL || vo == NULL) {
					perror("malloc");
					exit(EXIT_FAILURE);
				}

				ko->type = JSON_STRING;
				vo->type = attr_type;

				ko->parent = vo->parent = properties;
				ko->array = vo->array = NULL;
				ko->keys = vo->keys = NULL;
				ko->values = vo->values = NULL;
				ko->parser = vo->parser = properties->parser;

				ko->string = strdup(k.c_str());
				vo->string = strdup(v.c_str());

				if (ko->string == NULL || vo->string == NULL) {
					perror("strdup");
					exit(EXIT_FAILURE);
				}

				ko->length = strlen(ko->string);
				vo->length = strlen(vo->string);
				vo->number = atof(vo->string);

				properties->keys[properties->length] = ko;
				properties->values[properties->length] = vo;
				properties->length++;
			}
		}
	}
}

struct json_join_action : json_feature_action {
	int add_feature(json_object *geometry, bool, json_object *, json_object *, json_object *, json_object *feature) {
		if (feature != geometry) {  // a real feature, not a bare geometry
			if (csvfile != NULL) {
				join_csv(feature);
			}

			char *s = json_stringify(feature);
			out(s, 1, json_hash_get(feature, "properties"));
			free(s);
		} else {
			char *s = json_stringify(geometry);
			out(s, 2, NULL);
			free(s);
		}

		return 1;
	}

	void check_crs(json_object *) {
	}
};

void process(FILE *fp, const char *fname) {
	json_pull *jp = json_begin_file(fp);

	json_join_action jja;
	jja.fname = fname;
	parse_json(&jja, jp);
	json_end(jp);
}

int main(int argc, char **argv) {
	const char *csv = NULL;

	struct option long_options[] = {
		{"wrap", no_argument, 0, 'w'},
		{"extract", required_argument, 0, 'e'},
		{"csv", required_argument, 0, 'c'},
		{"empty-csv-columns-are-null", no_argument, &pe, 1},
		{"prevent", required_argument, 0, 'p'},

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
		case 0:
			break;

		case 'w':
			wrap = true;
			break;

		case 'e':
			extract = optarg;
			break;

		case 'c':
			csv = optarg;
			break;

		case 'p':
			if (strcmp(optarg, "e") == 0) {
				pe = true;
			} else {
				fprintf(stderr, "%s: Unknown option for -p%s\n", argv[0], optarg);
				exit(EXIT_FAILURE);
			}
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
