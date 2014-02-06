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
} json_object;

json_object *json_parse(FILE *f, json_object *current);
json_object *json_hash_get(json_object *o, char *s);
