#define GEOM_POINT 0	   /* array of positions */
#define GEOM_MULTIPOINT 1      /* array of arrays of positions */
#define GEOM_LINESTRING 2      /* array of arrays of positions */
#define GEOM_MULTILINESTRING 3 /* array of arrays of arrays of positions */
#define GEOM_POLYGON 4	 /* array of arrays of arrays of positions */
#define GEOM_MULTIPOLYGON 5    /* array of arrays of arrays of arrays of positions */
#define GEOM_TYPES 6

extern const char *geometry_names[GEOM_TYPES];
extern int32_t geometry_within[GEOM_TYPES];
extern int32_t mb_geometry[GEOM_TYPES];

void json_context(json_object *j);
void parse_geometry(int32_t t, json_object *j, drawvec &out, int32_t op, const char *fname, int32_t line, json_object *feature);

void stringify_value(json_object *value, int32_t &type, std::string &stringified, const char *reading, int32_t line, json_object *feature);
