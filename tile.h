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
struct json_pull;

void serialize_int(FILE *out, int n, long long *fpos, const char *fname, struct json_pull *source);
void serialize_long_long(FILE *out, long long n, long long *fpos, const char *fname, struct json_pull *source);
void serialize_byte(FILE *out, signed char n, long long *fpos, const char *fname, struct json_pull *source);
void serialize_uint(FILE *out, unsigned n, long long *fpos, const char *fname, struct json_pull *source);
void serialize_string(FILE *out, const char *s, long long *fpos, const char *fname, struct json_pull *source);

void deserialize_int(char **f, int *n);
void deserialize_long_long(char **f, long long *n);
void deserialize_uint(char **f, unsigned *n);
void deserialize_byte(char **f, signed char *n);
struct pool_val *deserialize_string(char **f, struct pool *p, int type);

long long write_tile(char **geom, char *metabase, unsigned *file_bbox, int z, unsigned x, unsigned y, int detail, int basezoom, struct pool *file_keys, const char *layername, sqlite3 *outdb, double droprate, int buffer, const char *fname, struct json_pull *jp, FILE *geomfile[4], int file_minzoom, int file_maxzoom, double todo, char *geomstart, long long along);
