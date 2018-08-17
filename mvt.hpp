#ifndef MVT_HPP
#define MVT_HPP

#include <sqlite3.h>
#include <string>
#include <map>
#include <set>
#include <vector>

struct mvt_value;
struct mvt_layer;

enum mvt_fmt {
	mvt_blake,
	mvt_original,
	mvt_reordered,
	mvt_blake_float,
};

extern int mvt_format;

enum mvt_operation {
	mvt_moveto = 1,
	mvt_lineto = 2,
	mvt_closepath = 7
};

struct mvt_geometry {
	long long x = 0;
	long long y = 0;
	int /* mvt_operation */ op = 0;

	mvt_geometry(int op, long long x, long long y);

	bool operator<(mvt_geometry const &s) const {
		if (y < s.y || (y == s.y && x < s.x)) {
			return true;
		} else {
			return false;
		}
	}

	bool operator==(mvt_geometry const &s) const {
		return y == s.y && x == s.x;
	}
};

enum mvt_geometry_type {
	mvt_point = 1,
	mvt_linestring = 2,
	mvt_polygon = 3
};

struct mvt_feature {
	std::vector<unsigned> tags{};
	std::vector<unsigned long> properties{};
	std::vector<mvt_geometry> geometry{};
	int /* mvt_geometry_type */ type = 0;
	unsigned long long id = 0;
	bool has_id = false;
	bool dropped = false;

	mvt_feature() {
		has_id = false;
		id = 0;
	}
};

enum mvt_value_type {
	mvt_string,
	mvt_float,
	mvt_double,
	mvt_int,
	mvt_uint,
	mvt_sint,
	mvt_bool,
	mvt_hash,
	mvt_list,
	mvt_null,
};

struct mvt_value {
	mvt_value_type type;
	std::string string_value;
	std::vector<size_t> list_value;
	union {
		float float_value;
		double double_value;
		long long int_value;
		unsigned long long uint_value;
		long long sint_value;
		bool bool_value;
		int null_value;
	} numeric_value;

	bool operator<(const mvt_value &o) const;
	std::string toString() const;

	mvt_value() {
		this->type = mvt_double;
		this->string_value = "";
		this->numeric_value.double_value = 0;
	}
};

struct mvt_attribute_pool {
	std::vector<std::string> keys{};
	std::vector<std::string> string_values{};
	std::vector<double> double_values{};
	std::vector<float> float_values{};
	std::vector<long> sint64_values{};
	std::vector<unsigned long> uint64_values{};
};

struct mvt_layer {
	int version = 0;
	std::string name = "";
	std::vector<mvt_feature> features{};
	std::vector<std::string> keys{};
	std::vector<mvt_value> values{};
	mvt_attribute_pool attribute_pool;
	long long extent = 0;

	// Add a key-value pair to a feature, using this layer's constant pool
	void tag(mvt_feature &feature, std::string key, mvt_value value);
	void tag_v3(mvt_feature &feature, std::string key, mvt_value value);
	size_t tag_value(mvt_value const &value);
	size_t tag_key(std::string const &key);
	size_t tag_v3_key(std::string key);
	void tag_v3_value(mvt_value value, std::vector<unsigned long> &onto);

	// For tracking the key-value constants already used in this layer
	std::map<std::string, size_t> key_map{};
	std::map<mvt_value, size_t> value_map{};
	std::map<mvt_value, unsigned long> property_map{};

	mvt_value decode_property(std::vector<unsigned long> const &property, size_t &off) const;
	void reorder_values();
};

struct mvt_tile {
	std::vector<mvt_layer> layers{};

	std::string encode();
	bool decode(std::string &message, bool &was_compressed);
};

bool is_compressed(std::string const &data);
int decompress(std::string const &input, std::string &output);
int compress(std::string const &input, std::string &output);
int dezig(unsigned n);

mvt_value stringified_to_mvt_value(int type, const char *s);

bool is_integer(const char *s, long long *v);
bool is_unsigned_integer(const char *s, unsigned long long *v);
#endif
