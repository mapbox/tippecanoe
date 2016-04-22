bool is_compressed(std::string const &data);
int decompress(std::string const &input, std::string &output);
int compress(std::string const &input, std::string &output);
int dezig(unsigned n);

enum pb_geometry_type {
	pb_point = 1, pb_linestring = 2, pb_polygon = 3
};

enum pb_operation {
	pb_moveto = 1, pb_lineto = 2, pb_closepath = 7
};

enum pb_value_type {
	pb_string, pb_float, pb_double, pb_int, pb_uint, pb_sint, pb_bool
};

struct pb_value {
	pb_value_type type;
	std::string string_value;
	union {
		float float_value;
		double double_value;
		long long int_value;
		unsigned long long uint_value;
		long long sint_value;
		bool bool_value;
	} numeric_value;
};

struct pb_geometry {
	int /* pb_operation */ op;
	long long x;
	long long y;

	pb_geometry(int op, long long x, long long y) {
		this->op = op;
		this->x = x;
		this->y = y;
	}
};

struct pb_feature {
	std::vector<unsigned> tags;
	int /* pb_geometry_type */ type;
	std::vector<pb_geometry> geometry;
};

struct pb_layer {
	int version;
	std::string name;
	std::vector<pb_feature> features;
	std::vector<std::string> keys;
	std::vector<pb_value> values;
	int extent;
};

struct pb_tile {
	std::vector<pb_layer> layers;
};

bool pb_decode(std::string &message, pb_tile &out);
