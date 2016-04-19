#define VT_POINT 1
#define VT_LINE 2
#define VT_POLYGON 3

#define VT_END 0
#define VT_MOVETO 1
#define VT_LINETO 2
#define VT_CLOSEPATH 7

#define VT_STRING 1
#define VT_NUMBER 2
#define VT_BOOLEAN 7

struct pool;

void serialize_int(FILE *out, int n, long long *fpos, const char *fname);
void serialize_long_long(FILE *out, long long n, long long *fpos, const char *fname);
void serialize_byte(FILE *out, signed char n, long long *fpos, const char *fname);
void serialize_uint(FILE *out, unsigned n, long long *fpos, const char *fname);
void serialize_string(FILE *out, const char *s, long long *fpos, const char *fname);

void deserialize_int(char **f, int *n);
void deserialize_long_long(char **f, long long *n);
void deserialize_uint(char **f, unsigned *n);
void deserialize_byte(char **f, signed char *n);

int deserialize_int_io(FILE *f, int *n, long long *geompos);
int deserialize_long_long_io(FILE *f, long long *n, long long *geompos);
int deserialize_uint_io(FILE *f, unsigned *n, long long *geompos);
int deserialize_byte_io(FILE *f, signed char *n, long long *geompos);

long long write_tile(char **geom, char *metabase, char *stringpool, unsigned *file_bbox, int z, unsigned x, unsigned y, int detail, int min_detail, int basezoom, struct pool **file_keys, char **layernames, sqlite3 *outdb, double droprate, int buffer, const char *fname, FILE **geomfile, int file_minzoom, int file_maxzoom, double todo, char *geomstart, long long along, double gamma, int nlayers, int *prevent, int *additional);

int traverse_zooms(int *geomfd, off_t *geom_size, char *metabase, char *stringpool, struct pool **file_keys, unsigned *midx, unsigned *midy, char **layernames, int maxzoom, int minzoom, int basezoom, sqlite3 *outdb, double droprate, int buffer, const char *fname, const char *tmpdir, double gamma, int nlayers, int *prevent, int *additional, int full_detail, int low_detail, int min_detail, long long *meta_off, long long *pool_off, unsigned *initial_x, unsigned *initial_y);

int manage_gap(unsigned long long index, unsigned long long *previndex, double scale, double gamma, double *gap);

extern int geometry_scale;
extern int quiet;

extern int CPUS;
extern int TEMP_FILES;

static int additional_options[] = {
#define A_COALESCE ((int) 'c')
	A_COALESCE,
#define A_REVERSE ((int) 'r')
	A_REVERSE,
#define A_REORDER ((int) 'o')
	A_REORDER,
#define A_LINE_DROP ((int) 'l')
	A_LINE_DROP,
#define A_POLYGON_DROP ((int) 'p')
	A_POLYGON_DROP,
#define A_PREFER_RADIX_SORT ((int) 'R')
	A_PREFER_RADIX_SORT,
#define A_DEBUG_POLYGON ((int) 'd')
	A_DEBUG_POLYGON,
};

static int prevent_options[] = {
#define P_SIMPLIFY ((int) 's')
	P_SIMPLIFY,
#define P_SIMPLIFY_LOW ((int) 'S')
	P_SIMPLIFY_LOW,
#define P_FEATURE_LIMIT ((int) 'f')
	P_FEATURE_LIMIT,
#define P_KILOBYTE_LIMIT ((int) 'k')
	P_KILOBYTE_LIMIT,
#define P_DYNAMIC_DROP ((int) 'd')
	P_DYNAMIC_DROP,
#define P_INPUT_ORDER ((int) 'i')
	P_INPUT_ORDER,
#define P_POLYGON_SPLIT ((int) 'p')
	P_POLYGON_SPLIT,
};
