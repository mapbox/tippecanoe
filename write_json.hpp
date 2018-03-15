#ifndef WRITE_JSON_HPP
#define WRITE_JSON_HPP

#include <string>
#include <vector>
#include <stdio.h>

enum json_write_tok {
	JSON_WRITE_HASH,
	JSON_WRITE_HASH_KEY,
	JSON_WRITE_HASH_VALUE,
	JSON_WRITE_ARRAY,
	JSON_WRITE_ARRAY_ELEMENT,
	JSON_WRITE_TOP,
};

struct json_write_state {
	std::vector<json_write_tok> state;
	bool nospace = false;
	bool wantnl = false;

	~json_write_state() {
		if (state.size() > 0) {
			if (state.size() != 1 || state[0] != JSON_WRITE_TOP) {
				fprintf(stderr, "JSON not closed at end\n");
				exit(EXIT_FAILURE);
			}
		}
	}
};

void layer_to_geojson(FILE *fp, mvt_layer const &layer, unsigned z, unsigned x, unsigned y, bool comma, bool name, bool zoom, bool dropped, unsigned long long index, long long sequence, long long extent, bool complain, json_write_state &state);
void fprintq(FILE *f, const char *s);

void json_write_array(FILE *f, json_write_state &state);
void json_end_array(FILE *f, json_write_state &state);
void json_write_hash(FILE *f, json_write_state &state);
void json_end_hash(FILE *f, json_write_state &state);
void json_write_string(FILE *f, std::string const &s, json_write_state &state);
void json_write_number(FILE *f, double d, json_write_state &state);
void json_write_float(FILE *f, double d, json_write_state &state);
void json_write_unsigned(FILE *f, unsigned long long v, json_write_state &state);
void json_write_signed(FILE *f, long long v, json_write_state &state);
void json_write_stringified(FILE *f, std::string const &s, json_write_state &state);
void json_write_bool(FILE *f, bool b, json_write_state &state);
void json_write_null(FILE *f, json_write_state &state);
void json_write_newline(FILE *f, json_write_state &state);
void json_comma_newline(FILE *f, json_write_state &state);

#endif
