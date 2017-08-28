#include <stdio.h>
#include <stdlib.h>
#include <map>
#include "mvt.hpp"
#include "evaluator.hpp"

int compare(mvt_value one, json_object *two, bool &fail) {
	if (one.type == mvt_string) {
		if (two->type != JSON_STRING) {
			fail = true;
			return false; // string vs non-string
		}

		return strcmp(one.string_value.c_str(), two->string);
	}

	if (one.type == mvt_double || one.type == mvt_float || one.type == mvt_int || one.type == mvt_uint || one.type == mvt_sint) {
		if (two->type != JSON_NUMBER) {
			fail = true;
			return false; // number vs non-number
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
			return false; // bool vs non-bool
		}

		bool b = two->type = JSON_TRUE;
		return one.numeric_value.bool_value > b;
	}

	fprintf(stderr, "Internal error: bad mvt type %d\n", one.type);
	exit(EXIT_FAILURE);
}

bool eval(std::map<std::string, mvt_value> const &feature, json_object *f) {
	if (f == NULL || f->type != JSON_ARRAY) {
		fprintf(stderr, "Filter is not an array: %s\n", json_stringify(f));
		exit(EXIT_FAILURE);
	}

	if (f->length < 2) {
		fprintf(stderr, "Array too small in filter: %s\n", json_stringify(f));
		exit(EXIT_FAILURE);
	}

	if (f->array[0]->type != JSON_STRING) {
		fprintf(stderr, "Filter operation is not a string: %s\n", json_stringify(f));
		exit(EXIT_FAILURE);
	}

	if (strcmp(f->array[0]->string, "has") == 0) {
		if (f->array[1]->type != JSON_STRING) {
			fprintf(stderr, "\"has\" key is not a string: %s\n", json_stringify(f));
			exit(EXIT_FAILURE);
		}
		return feature.count(std::string(f->array[1]->string)) != 0;
	}

	if (strcmp(f->array[0]->string, "!has") == 0) {
		if (f->array[1]->type != JSON_STRING) {
			fprintf(stderr, "\"!has\" key is not a string: %s\n", json_stringify(f));
			exit(EXIT_FAILURE);
		}
		return feature.count(std::string(f->array[1]->string)) == 0;
	}

	if (strcmp(f->array[0]->string, "==") == 0 ||
	    strcmp(f->array[0]->string, "!=") == 0 ||
	    strcmp(f->array[0]->string, ">") == 0 ||
	    strcmp(f->array[0]->string, ">=") == 0 ||
	    strcmp(f->array[0]->string, "<") == 0 ||
	    strcmp(f->array[0]->string, "<=") == 0) {
		if (f->length < 3) {
			fprintf(stderr, "Array too small in filter: %s\n", json_stringify(f));
			exit(EXIT_FAILURE);
		}
		if (f->array[1]->type != JSON_STRING) {
			fprintf(stderr, "\"!has\" key is not a string: %s\n", json_stringify(f));
			exit(EXIT_FAILURE);
		}

		auto ff = feature.find(std::string(f->array[1]->string));
		if (ff == feature.end()) {
			return false; // not found: comparison is false
		}

		bool fail = false;
		int cmp = compare(ff->second, f->array[2], fail);

		if (fail) {
			return false;
		}

		if (strcmp(f->array[0]->string, "==") == 0) {
			return cmp == 0;
		}
		if (strcmp(f->array[0]->string, "!=") == 0) {
			return cmp != 0;
		}
		if (strcmp(f->array[0]->string, ">") == 0) {
			return cmp > 0;
		}
		if (strcmp(f->array[0]->string, ">=") == 0) {
			return cmp >= 0;
		}
		if (strcmp(f->array[0]->string, "<") == 0) {
			return cmp < 0;
		}
		if (strcmp(f->array[0]->string, "<=") == 0) {
			return cmp <= 0;
		}

		fprintf(stderr, "Internal error: can't happen: %s\n", json_stringify(f));
		exit(EXIT_FAILURE);
	}

	if (strcmp(f->array[0]->string, "all") == 0 ||
	    strcmp(f->array[0]->string, "any") == 0 ||
	    strcmp(f->array[0]->string, "none") == 0) {
		bool v;

		if (strcmp(f->array[0]->string, "all") == 0) {
			v = true;
		} else {
			v = false;
		}

		for (size_t i = 1; i < f->length; i++) {
			bool out = eval(feature, f->array[i]);

			if (strcmp(f->array[0]->string, "all") == 0) {
				v = v && out;
			} else {
				v = v || out;
			}
		}

		if (strcmp(f->array[0]->string, "none") == 0) {
			return !v;
		} else {
			return v;
		}
	}

	fprintf(stderr, "Unknown filter %s\n", json_stringify(f));
	exit(EXIT_FAILURE);
}

bool evaluate(std::map<std::string, mvt_value> const &feature, std::string const &layer, json_object *filter) {
	json_object *f = json_hash_get(filter, layer.c_str());

	if (f != NULL) {
		return eval(feature, f);
	} else {
		return true; // no filter for this layer;
	}
}
