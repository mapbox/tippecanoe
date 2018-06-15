#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <string>
#include <getopt.h>
#include <vector>
#include "jsonpull/jsonpull.hpp"
#include "csv.hpp"
#include "text.hpp"

int fail = EXIT_SUCCESS;
bool wrap = false;
const char *extract = NULL;

FILE *csvfile = NULL;
std::vector<std::string> header;
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

void out(std::string const &s, int type, std::shared_ptr<json_object> properties) {
	if (extract != NULL) {
		std::string extracted = sort_quote("null");
		bool found = false;

		std::shared_ptr<json_object> o = json_hash_get(properties, extract);
		if (o != NULL) {
			found = true;
			if (o->type == JSON_STRING || o->type == JSON_NUMBER) {
				extracted = sort_quote(o->string.c_str());
			} else {
				// Don't really know what to do about sort quoting
				// for arbitrary objects

				std::string out = json_stringify(o);
				extracted = sort_quote(out.c_str());
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

void join_csv(std::shared_ptr<json_object> j) {
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

	std::shared_ptr<json_object> properties = json_hash_get(j, "properties");
	std::shared_ptr<json_object> key = NULL;

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
		std::string s = json_stringify(key);
		joinkey = s;
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
			}

			{
				// This knows more about the structure of JSON objects than it ought to

				std::shared_ptr<json_object> ko = std::make_shared<json_object>();
				std::shared_ptr<json_object> vo = std::make_shared<json_object>();
				if (ko == NULL || vo == NULL) {
					perror("malloc");
					exit(EXIT_FAILURE);
				}

				ko->type = JSON_STRING;
				vo->type = attr_type;

				ko->parent = vo->parent = properties;
				ko->parser = vo->parser = properties->parser;

				ko->string = k;
				vo->string = v;
				vo->number = atof(vo->string.c_str());

				properties->keys.push_back(ko);
				properties->values.push_back(vo);
			}
		}
	}
}

void process(FILE *fp, const char *fname) {
	std::shared_ptr<json_pull> jp = json_begin_file(fp);

	while (1) {
		std::shared_ptr<json_object> j = json_read(jp);
		if (j == NULL) {
			if (jp->error.size() != 0) {
				fprintf(stderr, "%s:%zu: %s\n", fname, jp->line, jp->error.c_str());
			}

			json_free(jp->root);
			break;
		}

		std::shared_ptr<json_object> type = json_hash_get(j, "type");
		if (type == NULL || type->type != JSON_STRING) {
			continue;
		}

		if (type->string == "Feature") {
			if (csvfile != NULL) {
				join_csv(j);
			}

			std::string s = json_stringify(j);
			out(s.c_str(), 1, json_hash_get(j, "properties"));
			json_free(j);
		} else if (type->string == "Point" ||
			   type->string == "MultiPoint" ||
			   type->string == "LineString" ||
			   type->string == "MultiLineString" ||
			   type->string == "MultiPolygon") {
			int is_geometry = 1;

			std::shared_ptr<json_object> parent = j->parent.lock();
			if (parent.use_count() != 0) {
				if (parent->type == JSON_ARRAY) {
					std::shared_ptr<json_object> parent_parent = parent->parent.lock();

					if (parent_parent.use_count() != 0 && parent_parent->type == JSON_HASH) {
						std::shared_ptr<json_object> geometries = json_hash_get(parent_parent, "geometries");
						if (geometries != NULL) {
							// Parent of Parent must be a GeometryCollection
							is_geometry = 0;
						}
					}
				} else if (parent->type == JSON_HASH) {
					std::shared_ptr<json_object> geometry = json_hash_get(parent, "geometry");
					if (geometry != NULL) {
						// Parent must be a Feature
						is_geometry = 0;
					}
				}
			}

			if (is_geometry) {
				std::string s = json_stringify(j);
				out(s.c_str(), 2, NULL);
				json_free(j);
			}
		} else if (type->string == "FeatureCollection") {
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
