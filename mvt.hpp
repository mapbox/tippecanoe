#ifndef MVT_HPP
#define MVT_HPP

#include <sqlite3.h>
#include <string>
#include <map>
#include <set>
#include <vector>

struct mvt_value;
struct mvt_layer;

enum mvt_operation {
	mvt_moveto = 1,
	mvt_lineto = 2,
	mvt_closepath = 7
};

struct mvt_geometry {
	int64_t x = 0;
	int64_t y = 0;
	int32_t /* mvt_operation */ op = 0;

	mvt_geometry(int32_t op, int64_t x, int64_t y);

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
	std::vector<uint32_t> tags{};
	std::vector<mvt_geometry> geometry{};
	int32_t /* mvt_geometry_type */ type = 0;
	uint64_t id = 0;
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
	mvt_null,
};

struct mvt_value {
	mvt_value_type type;
	std::string string_value;
	union {
		float float_value;
		double double_value;
		int64_t int_value;
		uint64_t uint_value;
		int64_t sint_value;
		bool bool_value;
		int32_t null_value;
	} numeric_value;

	bool operator<(const mvt_value &o) const;
	std::string toString();

	mvt_value() {
		this->type = mvt_double;
		this->string_value = "";
		this->numeric_value.double_value = 0;
	}
};

struct mvt_layer {
	int32_t version = 0;
	std::string name = "";
	std::vector<mvt_feature> features{};
	std::vector<std::string> keys{};
	std::vector<mvt_value> values{};
	int64_t extent = 0;

	// Add a key-value pair to a feature, using this layer's constant pool
	void tag(mvt_feature &feature, std::string key, mvt_value value);

	// For tracking the key-value constants already used in this layer
	std::map<std::string, size_t> key_map{};
	std::map<mvt_value, size_t> value_map{};
};

struct mvt_tile {
	std::vector<mvt_layer> layers{};

	std::string encode();
	bool decode(std::string &message, bool &was_compressed);
};

bool is_compressed(std::string const &data);
int32_t decompress(std::string const &input, std::string &output);
int32_t compress(std::string const &input, std::string &output);
int32_t dezig(uint32_t n);

mvt_value stringified_to_mvt_value(int32_t type, const char *s);

bool is_integer(const char *s, int64_t *v);
bool is_uint32_t_integer(const char *s, uint64_t *v);
#endif
