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




struct pool_val {
	char *s;
	int type;
	int n;

	struct pool_val *left;
	struct pool_val *right;

	struct pool_val *next;
};

struct pool {
	struct pool_val *vals;

	struct pool_val *head;
	struct pool_val *tail;
	int n;
};


void deserialize_int(char **f, int *n);
struct pool_val *deserialize_string(char **f, struct pool *p, int type);
struct pool_val *pool(struct pool *p, char *s, int type);
struct pool_val *pool_long_long(struct pool *p, long long *val, int type);
void pool_free(struct pool *p);


struct index {
        unsigned long long index;
        long long fpos;

        struct index *next;
};


long long write_tile(struct index *start, struct index *end, char *metabase, unsigned *file_bbox, int z, unsigned x, unsigned y, int detail, int basezoom, struct pool *file_keys);
