struct mvt_value;
struct mvt_layer;

enum mvt_operation {
	mvt_moveto = 1,
	mvt_lineto = 2,
	mvt_closepath = 7
};

struct mvt_geometry {
	int x;
	int y;
	int /* mvt_operation */ op;

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
	std::vector<unsigned> tags;
	std::vector<mvt_geometry> geometry;
	int /* mvt_geometry_type */ type;
	unsigned long long id;
	bool has_id;

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
	mvt_bool
};

struct mvt_value {
	mvt_value_type type;
	std::string string_value;
	union {
		float float_value;
		double double_value;
		long long int_value;
		unsigned long long uint_value;
		long long sint_value;
		bool bool_value;
	} numeric_value;

	bool operator<(const mvt_value &o) const;
};

struct mvt_layer {
	int version;
	std::string name;
	std::vector<mvt_feature> features;
	std::vector<std::string> keys;
	std::vector<mvt_value> values;
	int extent;

	// Add a key-value pair to a feature, using this layer's constant pool
	void tag(mvt_feature &feature, std::string key, mvt_value value);

	// For tracking the key-value constants already used in this layer
	std::map<std::string, size_t> key_map;
	std::map<mvt_value, size_t> value_map;
};

struct mvt_tile {
	std::vector<mvt_layer> layers;

	std::string encode();
	bool decode(std::string &message);
};

bool is_compressed(std::string const &data);
int decompress(std::string const &input, std::string &output);
int compress(std::string const &input, std::string &output);
int dezig(unsigned n);
