#include <stdio.h>
#include <stdlib.h>

typedef enum json_type {
	JSON_STRING, JSON_NUMBER, JSON_ARRAY, JSON_OBJECT, JSON_NULL, JSON_TRUE, JSON_FALSE,
} json_type;

typedef struct json_array {
	struct json_object *object;
	struct json_array *next;
} json_array;

typedef struct json_hash {
	struct json_object *key;
	struct json_object *value;
	struct json_hash *next;
} json_hash;

typedef struct json_object {
	json_type type;
	struct json_object *parent;

	char *string;
	double number;
	json_array *array;
	json_hash *object;
} json_object;
