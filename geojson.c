#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include "jsonpull.h"

#define GEOM_POINT 0                /* array of positions */
#define GEOM_MULTIPOINT 1           /* array of arrays of positions */
#define GEOM_LINESTRING 2           /* array of arrays of positions */
#define GEOM_MULTILINESTRING 3      /* array of arrays of arrays of positions */
#define GEOM_POLYGON 4              /* array of arrays of arrays of positions */
#define GEOM_MULTIPOLYGON 5         /* array of arrays of arrays of arrays of positions */

#define GEOM_TYPES 6

char *geometry_names[GEOM_TYPES] = {
	"Point",
	"MultiPoint",
	"LineString",
	"MultiLineString",
	"Polygon",
	"MultiPolygon",
};

int geometry_depths[GEOM_TYPES] = {
	1,
	2,
	2,
	3,
	3,
	4,
};

/* XXX */
#define META_STRING JSON_STRING
#define META_INTEGER JSON_NUMBER
int metabits = 32;

void read_json(FILE *f) {
	json_pull *jp = json_begin_file(f);

	while (1) {
		json_object *j = json_read(jp);
		if (j == NULL) {
			if (jp->error != NULL) {
				fprintf(stderr, "%d: %s\n", jp->line, jp->error);
			}

			json_free(jp->root);
			break;
		}

		json_object *type = json_hash_get(j, "type");
		if (type == NULL || type->type != JSON_STRING || strcmp(type->string, "Feature") != 0) {
			continue;
		}

		json_object *geometry = json_hash_get(j, "geometry");
		if (geometry == NULL) {
			fprintf(stderr, "%d: feature with no geometry\n", jp->line);
			goto next_feature;
		}

		json_object *geometry_type = json_hash_get(geometry, "type");
		if (geometry_type == NULL || geometry_type->type != JSON_STRING) {
			fprintf(stderr, "%d: geometry without type string\n", jp->line);
			goto next_feature;
		}

		json_object *properties = json_hash_get(j, "properties");
		if (properties == NULL || properties->type != JSON_HASH) {
			fprintf(stderr, "%d: feature without properties hash\n", jp->line);
			goto next_feature;
		}

		json_object *coordinates = json_hash_get(geometry, "coordinates"); 
		if (coordinates == NULL || coordinates->type != JSON_ARRAY) {
			fprintf(stderr, "%d: feature without coordinates array\n", jp->line);
			goto next_feature;
		}

		int t;
		for (t = 0; t < GEOM_TYPES; t++) {
			if (strcmp(geometry_type->string, geometry_names[t]) == 0) {
				break;
			}
		}
		if (t >= GEOM_TYPES) {
			fprintf(stderr, "%d: Can't handle geometry type %s\n", jp->line, geometry_type->string);
			goto next_feature;
		}

		/* scope for variable-length arrays */
		{
			char *metakey[properties->length];
			char *metaval[properties->length];
			int metatype[properties->length];
			int m = 0;

			int i;
			for (i = 0; i < properties->length; i++) {
				if (properties->keys[i]->type == JSON_STRING) {
					metakey[m] = properties->keys[i]->string;

					if (properties->values[i] != NULL && properties->values[i]->type == JSON_STRING) {
						metatype[m] = META_STRING;
						metaval[m] = properties->values[i]->string;
						m++;
					} else if (properties->values[i] != NULL && properties->values[i]->type == JSON_NUMBER) {
						metatype[m] = META_INTEGER;
						metaval[m] = properties->values[i]->string;
						m++;
					} else {
						fprintf(stderr, "%d: Unsupported meta type\n", jp->line);
					}
				}
			}

			int n;
			if (t == GEOM_POINT) {
				n = 1;
			} else {
				n = coordinates->length;
			}

			double lon[n], lat[n];
			int ok = 1;

			if (t == GEOM_POINT) {
				if (coordinates->length >= 2) {
					lon[0] = coordinates->array[0]->number;
					lat[0] = coordinates->array[1]->number;
				} else {
					fprintf(stderr, "%d: not enough coordinates for a point\n", jp->line);
					ok = 0;
				}
			} else {
				for (i = 0; i < n; i++) {
					if (coordinates->array[i]->type == JSON_ARRAY &&
					    coordinates->array[i]->length >= 2) {
						lon[i] = coordinates->array[i]->array[0]->number;
						lat[i] = coordinates->array[i]->array[1]->number;
					} else {
						fprintf(stderr, "%d: not enough coordinates within the geometry\n", jp->line);
						ok = 0;
					}
				}
			}

			if (ok) {
				for (i = 0; i < n; i++) {
					printf("%f,%f ", lat[i], lon[i]);
				}
				printf("\n");
				// XXX encode(destdir, files, maxn, extra, xoff, pool, n, lat, lon, m, metakey, metaval, metatype);
			}
		}

next_feature:
		json_free(j);

		/* XXX check for any non-features in the outer object */
	}

	json_end(jp);
}

int main(int argc, char **argv) {
	if (argc > 1) {
		int i;
		for (i = 1; i < argc; i++) {
			FILE *f = fopen(argv[i], "r");
			if (f == NULL) {
				fprintf(stderr, "%s: %s: %s\n", argv[0], argv[i], strerror(errno));
			} else {
				read_json(f);
				fclose(f);
			}
		}
	} else {
		read_json(stdin);
	}
	return 0;
}
