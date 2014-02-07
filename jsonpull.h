typedef enum json_type {
	JSON_HASH, JSON_ARRAY, JSON_NUMBER, JSON_STRING, JSON_TRUE, JSON_FALSE, JSON_NULL,
} json_type;

typedef struct json_object {
	json_type type;
	struct json_object *parent;

	char *string;
	double number;

	struct json_object **array;
	struct json_object **keys;
	struct json_object **values;
	int length;

	int expect;
} json_object;

struct json_pull {
	json_object *container;
	char *error;
	FILE *f;
};
typedef struct json_pull json_pull;

json_pull *json_begin(FILE *f);
json_object *json_parse(json_pull *j);

json_object *json_hash_get(json_object *o, char *s);
