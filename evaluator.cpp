#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include "mvt.hpp"
#include "evaluator.hpp"

int compare(mvt_value one, json_object *two, bool &fail) {
	if (one.type == mvt_string) {
		if (two->type != JSON_STRING) {
			fail = true;
			return false;  // string vs non-string
		}

		return strcmp(one.string_value.c_str(), two->string.c_str());
	}

	if (one.type == mvt_double || one.type == mvt_float || one.type == mvt_int || one.type == mvt_uint || one.type == mvt_sint) {
		if (two->type != JSON_NUMBER) {
			fail = true;
			return false;  // number vs non-number
		}

		double v;
		if (one.type == mvt_double) {
			v = one.numeric_value.double_value;
		} else if (one.type == mvt_float) {
			v = one.numeric_value.float_value;
		} else if (one.type == mvt_int) {
			v = one.numeric_value.int_value;
		} else if (one.type == mvt_uint) {
			v = one.numeric_value.uint_value;
		} else if (one.type == mvt_sint) {
			v = one.numeric_value.sint_value;
		} else {
			fprintf(stderr, "Internal error: bad mvt type %d\n", one.type);
			exit(EXIT_FAILURE);
		}

		if (v < two->number) {
			return -1;
		} else if (v > two->number) {
			return 1;
		} else {
			return 0;
		}
	}

	if (one.type == mvt_bool) {
		if (two->type != JSON_TRUE && two->type != JSON_FALSE) {
			fail = true;
			return false;  // bool vs non-bool
		}

		bool b = two->type != JSON_FALSE;
		return one.numeric_value.bool_value > b;
	}

	if (one.type == mvt_null) {
		if (two->type != JSON_NULL) {
			fail = true;
			return false;  // null vs non-null
		}

		return 0;  // null equals null
	}

	fprintf(stderr, "Internal error: bad mvt type %d\n", one.type);
	exit(EXIT_FAILURE);
}

bool eval(std::map<std::string, mvt_value> const &feature, json_object *f) {
	if (f == NULL || f->type != JSON_ARRAY) {
		fprintf(stderr, "Filter is not an array: %s\n", json_stringify(f).c_str());
		exit(EXIT_FAILURE);
	}

	if (f->array.size() < 1) {
		fprintf(stderr, "Array too small in filter: %s\n", json_stringify(f).c_str());
		exit(EXIT_FAILURE);
	}

	if (f->array[0]->type != JSON_STRING) {
		fprintf(stderr, "Filter operation is not a string: %s\n", json_stringify(f).c_str());
		exit(EXIT_FAILURE);
	}

	if (f->array[0]->string == "has" ||
	    f->array[0]->string == "!has") {
		if (f->array.size() != 2) {
			fprintf(stderr, "Wrong number of array elements in filter: %s\n", json_stringify(f).c_str());
			exit(EXIT_FAILURE);
		}

		if (f->array[0]->string == "has") {
			if (f->array[1]->type != JSON_STRING) {
				fprintf(stderr, "\"has\" key is not a string: %s\n", json_stringify(f).c_str());
				exit(EXIT_FAILURE);
			}
			return feature.count(std::string(f->array[1]->string)) != 0;
		}

		if (f->array[0]->string == "!has") {
			if (f->array[1]->type != JSON_STRING) {
				fprintf(stderr, "\"!has\" key is not a string: %s\n", json_stringify(f).c_str());
				exit(EXIT_FAILURE);
			}
			return feature.count(std::string(f->array[1]->string)) == 0;
		}
	}

	if (f->array[0]->string == "==" ||
	    f->array[0]->string == "!=" ||
	    f->array[0]->string == ">" ||
	    f->array[0]->string == ">=" ||
	    f->array[0]->string == "<" ||
	    f->array[0]->string == "<=") {
		if (f->array.size() != 3) {
			fprintf(stderr, "Wrong number of array elements in filter: %s\n", json_stringify(f).c_str());
			exit(EXIT_FAILURE);
		}
		if (f->array[1]->type != JSON_STRING) {
			fprintf(stderr, "\"!has\" key is not a string: %s\n", json_stringify(f).c_str());
			exit(EXIT_FAILURE);
		}

		auto ff = feature.find(std::string(f->array[1]->string));
		if (ff == feature.end()) {
			static bool warned = false;
			if (!warned) {
				std::string s = json_stringify(f);
				fprintf(stderr, "Warning: attribute not found for comparison: %s\n", s.c_str());
				warned = true;
			}
			if (f->array[0]->string == "!=") {
				return true;  //  attributes that aren't found are not equal
			}
			return false;  // not found: comparison is false
		}

		bool fail = false;
		int cmp = compare(ff->second, f->array[2], fail);

		if (fail) {
			static bool warned = false;
			if (!warned) {
				std::string s = json_stringify(f);
				fprintf(stderr, "Warning: mismatched type in comparison: %s\n", s.c_str());
				warned = true;
			}
			if (f->array[0]->string == "!=") {
				return true;  // mismatched types are not equal
			}
			return false;
		}

		if (f->array[0]->string == "==") {
			return cmp == 0;
		}
		if (f->array[0]->string == "!=") {
			return cmp != 0;
		}
		if (f->array[0]->string == ">") {
			return cmp > 0;
		}
		if (f->array[0]->string == ">=") {
			return cmp >= 0;
		}
		if (f->array[0]->string == "<") {
			return cmp < 0;
		}
		if (f->array[0]->string == "<=") {
			return cmp <= 0;
		}

		fprintf(stderr, "Internal error: can't happen: %s\n", json_stringify(f).c_str());
		exit(EXIT_FAILURE);
	}

	if (f->array[0]->string == "all" ||
	    f->array[0]->string == "any" ||
	    f->array[0]->string == "none") {
		bool v;

		if (f->array[0]->string == "all") {
			v = true;
		} else {
			v = false;
		}

		for (size_t i = 1; i < f->array.size(); i++) {
			bool out = eval(feature, f->array[i]);

			if (f->array[0]->string == "all") {
				v = v && out;
				if (!v) {
					break;
				}
			} else {
				v = v || out;
				if (v) {
					break;
				}
			}
		}

		if (f->array[0]->string == "none") {
			return !v;
		} else {
			return v;
		}
	}

	if (f->array[0]->string == "in" ||
	    f->array[0]->string == "!in") {
		if (f->array.size() < 2) {
			fprintf(stderr, "Array too small in filter: %s\n", json_stringify(f).c_str());
			exit(EXIT_FAILURE);
		}

		if (f->array[1]->type != JSON_STRING) {
			fprintf(stderr, "\"!has\" key is not a string: %s\n", json_stringify(f).c_str());
			exit(EXIT_FAILURE);
		}

		auto ff = feature.find(std::string(f->array[1]->string));
		if (ff == feature.end()) {
			static bool warned = false;
			if (!warned) {
				std::string s = json_stringify(f);
				fprintf(stderr, "Warning: attribute not found for comparison: %s\n", s.c_str());
				warned = true;
			}
			if (f->array[0]->string == "!in") {
				return true;  // attributes that aren't found are not in
			}
			return false;  // not found: comparison is false
		}

		bool found = false;
		for (size_t i = 2; i < f->array.size(); i++) {
			bool fail = false;
			int cmp = compare(ff->second, f->array[i], fail);

			if (fail) {
				static bool warned = false;
				if (!warned) {
					std::string s = json_stringify(f);
					fprintf(stderr, "Warning: mismatched type in comparison: %s\n", s.c_str());
					warned = true;
				}
				cmp = 1;
			}

			if (cmp == 0) {
				found = true;
				break;
			}
		}

		if (f->array[0]->string == "in") {
			return found;
		} else {
			return !found;
		}
	}

	fprintf(stderr, "Unknown filter %s\n", json_stringify(f).c_str());
	exit(EXIT_FAILURE);
}

bool evaluate(std::map<std::string, mvt_value> const &feature, std::string const &layer, json_object *filter) {
	if (filter == NULL || filter->type != JSON_HASH) {
		fprintf(stderr, "Error: filter is not a hash: %s\n", json_stringify(filter).c_str());
		exit(EXIT_FAILURE);
	}

	bool ok = true;
	json_object *f;

	f = json_hash_get(filter, layer.c_str());
	if (ok && f != NULL) {
		ok = eval(feature, f);
	}

	f = json_hash_get(filter, "*");
	if (ok && f != NULL) {
		ok = eval(feature, f);
	}

	return ok;
}

json_object *read_filter(const char *fname) {
	FILE *fp = fopen(fname, "r");
	if (fp == NULL) {
		perror(fname);
		exit(EXIT_FAILURE);
	}

	json_pull *jp = json_begin_file(fp);
	json_object *filter = json_read_tree(jp);
	if (filter == NULL) {
		fprintf(stderr, "%s: %s\n", fname, jp->error.c_str());
		exit(EXIT_FAILURE);
	}
	json_disconnect(filter);
	json_end(jp);
	fclose(fp);
	return filter;
}

json_object *parse_filter(const char *s) {
	json_pull *jp = json_begin_string(s);
	json_object *filter = json_read_tree(jp);
	if (filter == NULL) {
		fprintf(stderr, "Could not parse filter %s\n", s);
		fprintf(stderr, "%s\n", jp->error.c_str());
		exit(EXIT_FAILURE);
	}
	json_disconnect(filter);
	json_end(jp);
	return filter;
}
