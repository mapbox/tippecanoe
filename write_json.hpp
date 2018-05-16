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

struct json_writer {
	std::vector<json_write_tok> state;
	bool nospace = false;
	bool wantnl = false;
	FILE *f = NULL;
	std::string *s = NULL;

	~json_writer() {
		if (state.size() > 0) {
			if (state.size() != 1 || state[0] != JSON_WRITE_TOP) {
				fprintf(stderr, "JSON not closed at end\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	json_writer(FILE *fp) {
		f = fp;
	}

	json_writer(std::string *out) {
		s = out;
	}

	void json_write_array();
	void json_end_array();
	void json_write_hash();
	void json_end_hash();
	void json_write_string(std::string const &s);
	void json_write_number(double d);
	void json_write_float(double d);
	void json_write_unsigned(unsigned long long v);
	void json_write_signed(long long v);
	void json_write_stringified(std::string const &s);
	void json_write_bool(bool b);
	void json_write_null();
	void json_write_newline();
	void json_comma_newline();

       private:
	void json_adjust();
	void aprintf(const char *format, ...);
	void addc(char c);
	void adds(std::string const &s);
};

void layer_to_geojson(mvt_layer const &layer, unsigned z, unsigned x, unsigned y, bool comma, bool name, bool zoom, bool dropped, unsigned long long index, long long sequence, long long extent, bool complain, json_writer &state);
void fprintq(FILE *f, const char *s);

#endif
