size_t fwrite_check(const void *ptr, size_t size, size_t nitems, FILE *stream, const char *fname);

void serialize_int(FILE *out, int n, long long *fpos, const char *fname);
void serialize_long_long(FILE *out, long long n, long long *fpos, const char *fname);
void serialize_ulong_long(FILE *out, unsigned long long n, long long *fpos, const char *fname);
void serialize_byte(FILE *out, signed char n, long long *fpos, const char *fname);
void serialize_uint(FILE *out, unsigned n, long long *fpos, const char *fname);
void serialize_string(FILE *out, const char *s, long long *fpos, const char *fname);

void deserialize_int(char **f, int *n);
void deserialize_long_long(char **f, long long *n);
void deserialize_ulong_long(char **f, unsigned long long *n);
void deserialize_uint(char **f, unsigned *n);
void deserialize_byte(char **f, signed char *n);

int deserialize_int_io(FILE *f, int *n, long long *geompos);
int deserialize_long_long_io(FILE *f, long long *n, long long *geompos);
int deserialize_ulong_long_io(FILE *f, unsigned long long *n, long long *geompos);
int deserialize_uint_io(FILE *f, unsigned *n, long long *geompos);
int deserialize_byte_io(FILE *f, signed char *n, long long *geompos);

struct serial_val {
	int type;
	std::string s;
};

struct serial_feature {
	long long layer;
	int segment;
	long long seq;

	signed char t;
	signed char feature_minzoom;

	bool has_id;
	unsigned long long id;

	bool has_tippecanoe_minzoom;
	int tippecanoe_minzoom;

	bool has_tippecanoe_maxzoom;
	int tippecanoe_maxzoom;

	drawvec geometry;
	unsigned long long index;
	long long extent;

	size_t m;
	std::vector<long long> keys;
	std::vector<long long> values;
	long long metapos;

	// XXX This isn't serialized. Should it be here?
	long long bbox[4];
	std::map<std::string, serial_val> kv;
};

void serialize_feature(FILE *geomfile, serial_feature *sf, long long *geompos, const char *fname, long long wx, long long wy, bool include_minzoom);
serial_feature deserialize_feature(FILE *geoms, long long *geompos_in, char *metabase, long long *meta_off, unsigned z, unsigned tx, unsigned ty, unsigned *initial_x, unsigned *initial_y);
