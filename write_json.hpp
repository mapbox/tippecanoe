#ifndef WRITE_JSON_HPP
#define WRITE_JSON_HPP

#include <string>
#include <vector>
#include <stdio.h>

enum write_tok {
	JSON_WRITE_HASH,
	JSON_WRITE_HASH_KEY,
	JSON_WRITE_HASH_VALUE,
	JSON_WRITE_ARRAY,
	JSON_WRITE_ARRAY_ELEMENT,
	JSON_WRITE_TOP,
};

struct json_writer {
	std::vector<write_tok> jw;
	bool nospace = false;
	bool wantnl = false;
	FILE *f = NULL;
	std::string *s = NULL;

	~json_writer() {
		if (jw.size() > 0) {
			if (jw.size() != 1 || jw[0] != JSON_WRITE_TOP) {
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

	void begin_array();
	void end_array();
	void begin_hash();
	void end_hash();
	void write_string(std::string const &s);
	void write_number(double d);
	void write_float(double d);
	void write_unsigned(unsigned long long v);
	void write_signed(long long v);
	void write_stringified(std::string const &s);
	void write_bool(bool b);
	void write_null();
	void write_newline();
	void json_comma_newline();

       private:
	void json_adjust();
	void aprintf(const char *format, ...);
	void addc(char c);
	void adds(std::string const &s);
};

void layer_to_geojson(mvt_layer const &layer, unsigned z, unsigned x, unsigned y, bool comma, bool name, bool zoom, bool dropped, unsigned long long index, long long sequence, long long extent, bool complain, json_writer &jw);
void fprintq(FILE *f, const char *s);

#endif
