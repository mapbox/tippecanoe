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

void deserialize_int(char **f, int *n);
struct pool_val *deserialize_string(char **f, struct pool *p, int type);


struct index {
        unsigned long long index;
        long long fpos;
	int maxzoom;
};

long long write_tile(struct index *start, struct index *end, char *metabase, unsigned *file_bbox, int z, unsigned x, unsigned y, int detail, int basezoom, struct pool *file_keys, char *layername, sqlite3 *outdb, double droprate);
