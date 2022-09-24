#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include "mvt.hpp"
#include "evaluator.hpp"
#include "errors.hpp"

int compare(mvt_value one, json_object *two, bool &fail) {
	if (one.type == mvt_string) {
		if (two->type != JSON_STRING) {
			fail = true;
			return false;  // string vs non-string
		}

		return strcmp(one.string_value.c_str(), two->value.string.string);
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
			exit(EXIT_IMPOSSIBLE);
		}

		if (v < two->value.number.number) {
			return -1;
		} else if (v > two->value.number.number) {
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
	exit(EXIT_IMPOSSIBLE);
}

bool eval(std::map<std::string, mvt_value> const &feature, json_object *f, std::set<std::string> &exclude_attributes) {
	if (f == NULL || f->type != JSON_ARRAY) {
		fprintf(stderr, "Filter is not an array: %s\n", json_stringify(f));
		exit(EXIT_FILTER);
	}

	if (f->value.array.length < 1) {
		fprintf(stderr, "Array too small in filter: %s\n", json_stringify(f));
		exit(EXIT_FILTER);
	}

	if (f->value.array.array[0]->type != JSON_STRING) {
		fprintf(stderr, "Filter operation is not a string: %s\n", json_stringify(f));
		exit(EXIT_FILTER);
	}

	if (strcmp(f->value.array.array[0]->value.string.string, "has") == 0 ||
	    strcmp(f->value.array.array[0]->value.string.string, "!has") == 0) {
		if (f->value.array.length != 2) {
			fprintf(stderr, "Wrong number of array elements in filter: %s\n", json_stringify(f));
			exit(EXIT_FILTER);
		}

		if (strcmp(f->value.array.array[0]->value.string.string, "has") == 0) {
			if (f->value.array.array[1]->type != JSON_STRING) {
				fprintf(stderr, "\"has\" key is not a string: %s\n", json_stringify(f));
				exit(EXIT_FILTER);
			}
			return feature.count(std::string(f->value.array.array[1]->value.string.string)) != 0;
		}

		if (strcmp(f->value.array.array[0]->value.string.string, "!has") == 0) {
			if (f->value.array.array[1]->type != JSON_STRING) {
				fprintf(stderr, "\"!has\" key is not a string: %s\n", json_stringify(f));
				exit(EXIT_FILTER);
			}
			return feature.count(std::string(f->value.array.array[1]->value.string.string)) == 0;
		}
	}

	if (strcmp(f->value.array.array[0]->value.string.string, "==") == 0 ||
	    strcmp(f->value.array.array[0]->value.string.string, "!=") == 0 ||
	    strcmp(f->value.array.array[0]->value.string.string, ">") == 0 ||
	    strcmp(f->value.array.array[0]->value.string.string, ">=") == 0 ||
	    strcmp(f->value.array.array[0]->value.string.string, "<") == 0 ||
	    strcmp(f->value.array.array[0]->value.string.string, "<=") == 0) {
		if (f->value.array.length != 3) {
			fprintf(stderr, "Wrong number of array elements in filter: %s\n", json_stringify(f));
			exit(EXIT_FILTER);
		}
		if (f->value.array.array[1]->type != JSON_STRING) {
			fprintf(stderr, "comparison key is not a string: %s\n", json_stringify(f));
			exit(EXIT_FILTER);
		}

		auto ff = feature.find(std::string(f->value.array.array[1]->value.string.string));
		if (ff == feature.end()) {
			static bool warned = false;
			if (!warned) {
				const char *s = json_stringify(f);
				fprintf(stderr, "Warning: attribute not found for comparison: %s\n", s);
				free((void *) s);
				warned = true;
			}
			if (strcmp(f->value.array.array[0]->value.string.string, "!=") == 0) {
				return true;  //  attributes that aren't found are not equal
			}
			return false;  // not found: comparison is false
		}

		bool fail = false;
		int cmp = compare(ff->second, f->value.array.array[2], fail);

		if (fail) {
			static bool warned = false;
			if (!warned) {
				const char *s = json_stringify(f);
				fprintf(stderr, "Warning: mismatched type in comparison: %s\n", s);
				free((void *) s);
				warned = true;
			}
			if (strcmp(f->value.array.array[0]->value.string.string, "!=") == 0) {
				return true;  // mismatched types are not equal
			}
			return false;
		}

		if (strcmp(f->value.array.array[0]->value.string.string, "==") == 0) {
			return cmp == 0;
		}
		if (strcmp(f->value.array.array[0]->value.string.string, "!=") == 0) {
			return cmp != 0;
		}
		if (strcmp(f->value.array.array[0]->value.string.string, ">") == 0) {
			return cmp > 0;
		}
		if (strcmp(f->value.array.array[0]->value.string.string, ">=") == 0) {
			return cmp >= 0;
		}
		if (strcmp(f->value.array.array[0]->value.string.string, "<") == 0) {
			return cmp < 0;
		}
		if (strcmp(f->value.array.array[0]->value.string.string, "<=") == 0) {
			return cmp <= 0;
		}

		fprintf(stderr, "Internal error: can't happen: %s\n", json_stringify(f));
		exit(EXIT_IMPOSSIBLE);
	}

	if (strcmp(f->value.array.array[0]->value.string.string, "all") == 0 ||
	    strcmp(f->value.array.array[0]->value.string.string, "any") == 0 ||
	    strcmp(f->value.array.array[0]->value.string.string, "none") == 0) {
		bool v;

		if (strcmp(f->value.array.array[0]->value.string.string, "all") == 0) {
			v = true;
		} else {
			v = false;
		}

		for (size_t i = 1; i < f->value.array.length; i++) {
			bool out = eval(feature, f->value.array.array[i], exclude_attributes);

			if (strcmp(f->value.array.array[0]->value.string.string, "all") == 0) {
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

		if (strcmp(f->value.array.array[0]->value.string.string, "none") == 0) {
			return !v;
		} else {
			return v;
		}
	}

	if (strcmp(f->value.array.array[0]->value.string.string, "in") == 0 ||
	    strcmp(f->value.array.array[0]->value.string.string, "!in") == 0) {
		if (f->value.array.length < 2) {
			fprintf(stderr, "Array too small in filter: %s\n", json_stringify(f));
			exit(EXIT_FILTER);
		}

		if (f->value.array.array[1]->type != JSON_STRING) {
			fprintf(stderr, "\"!in\" key is not a string: %s\n", json_stringify(f));
			exit(EXIT_FILTER);
		}

		auto ff = feature.find(std::string(f->value.array.array[1]->value.string.string));
		if (ff == feature.end()) {
			static bool warned = false;
			if (!warned) {
				const char *s = json_stringify(f);
				fprintf(stderr, "Warning: attribute not found for comparison: %s\n", s);
				free((void *) s);
				warned = true;
			}
			if (strcmp(f->value.array.array[0]->value.string.string, "!in") == 0) {
				return true;  // attributes that aren't found are not in
			}
			return false;  // not found: comparison is false
		}

		bool found = false;
		for (size_t i = 2; i < f->value.array.length; i++) {
			bool fail = false;
			int cmp = compare(ff->second, f->value.array.array[i], fail);

			if (fail) {
				static bool warned = false;
				if (!warned) {
					const char *s = json_stringify(f);
					fprintf(stderr, "Warning: mismatched type in comparison: %s\n", s);
					free((void *) s);
					warned = true;
				}
				cmp = 1;
			}

			if (cmp == 0) {
				found = true;
				break;
			}
		}

		if (strcmp(f->value.array.array[0]->value.string.string, "in") == 0) {
			return found;
		} else {
			return !found;
		}
	}

	if (strcmp(f->value.array.array[0]->value.string.string, "attribute-filter") == 0) {
		if (f->value.array.length != 3) {
			fprintf(stderr, "Wrong number of array elements in filter: %s\n", json_stringify(f));
			exit(EXIT_FILTER);
		}

		if (f->value.array.array[1]->type != JSON_STRING) {
			fprintf(stderr, "\"attribute-filter\" key is not a string: %s\n", json_stringify(f));
			exit(EXIT_FILTER);
		}

		bool ok = eval(feature, f->value.array.array[2], exclude_attributes);
		if (!ok) {
			exclude_attributes.insert(f->value.array.array[1]->value.string.string);
		}

		return true;
	}

	fprintf(stderr, "Unknown filter %s\n", json_stringify(f));
	exit(EXIT_FILTER);
}

bool evaluate(std::map<std::string, mvt_value> const &feature, std::string const &layer, json_object *filter, std::set<std::string> &exclude_attributes) {
	if (filter == NULL || filter->type != JSON_HASH) {
		fprintf(stderr, "Error: filter is not a hash: %s\n", json_stringify(filter));
		exit(EXIT_JSON);
	}

	bool ok = true;
	json_object *f;

	f = json_hash_get(filter, layer.c_str());
	if (ok && f != NULL) {
		ok = eval(feature, f, exclude_attributes);
	}

	f = json_hash_get(filter, "*");
	if (ok && f != NULL) {
		ok = eval(feature, f, exclude_attributes);
	}

	return ok;
}

json_object *read_filter(const char *fname) {
	FILE *fp = fopen(fname, "r");
	if (fp == NULL) {
		perror(fname);
		exit(EXIT_OPEN);
	}

	json_pull *jp = json_begin_file(fp);
	json_object *filter = json_read_tree(jp);
	if (filter == NULL) {
		fprintf(stderr, "%s: %s\n", fname, jp->error);
		exit(EXIT_JSON);
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
		fprintf(stderr, "%s\n", jp->error);
		exit(EXIT_JSON);
	}
	json_disconnect(filter);
	json_end(jp);
	return filter;
}
