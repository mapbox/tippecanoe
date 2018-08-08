#ifdef MTRACE
#include <mcheck.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include "geojson-loop.hpp"
#include "jsonpull/jsonpull.h"

// XXX duplicated
#define GEOM_TYPES 6
static const char *geometry_names[GEOM_TYPES] = {
	"Point", "MultiPoint", "LineString", "MultiLineString", "Polygon", "MultiPolygon",
};

// XXX duplicated
static void json_context(json_object *j) {
	char *s = json_stringify(j);

	if (strlen(s) >= 500) {
		sprintf(s + 497, "...");
	}

	fprintf(stderr, "In JSON object %s\n", s);
	free(s);  // stringify
}

void parse_json(json_feature_action *jfa, json_pull *jp) {
	long long found_hashes = 0;
	long long found_features = 0;
	long long found_geometries = 0;

	while (1) {
		json_object *j = json_read(jp);
		if (j == NULL) {
			if (jp->error != NULL) {
				fprintf(stderr, "%s:%d: %s\n", jfa->fname.c_str(), jp->line, jp->error);
				if (jp->root != NULL) {
					json_context(jp->root);
				}
			}

			json_free(jp->root);
			break;
		}

		if (j->type == JSON_HASH) {
			found_hashes++;

			if (found_hashes == 50 && found_features == 0 && found_geometries == 0) {
				fprintf(stderr, "%s:%d: Warning: not finding any GeoJSON features or geometries in input yet after 50 objects.\n", jfa->fname.c_str(), jp->line);
			}
		}

		json_object *type = json_hash_get(j, "type");
		if (type == NULL || type->type != JSON_STRING) {
			continue;
		}

		if (found_features == 0) {
			int i;
			int is_geometry = 0;
			for (i = 0; i < GEOM_TYPES; i++) {
				if (strcmp(type->string, geometry_names[i]) == 0) {
					is_geometry = 1;
					break;
				}
			}

			if (is_geometry) {
				if (j->parent != NULL) {
					if (j->parent->type == JSON_ARRAY && j->parent->parent != NULL) {
						if (j->parent->parent->type == JSON_HASH) {
							json_object *geometries = json_hash_get(j->parent->parent, "geometries");
							if (geometries != NULL) {
								// Parent of Parent must be a GeometryCollection
								is_geometry = 0;
							}
						}
					} else if (j->parent->type == JSON_HASH) {
						json_object *geometry = json_hash_get(j->parent, "geometry");
						if (geometry != NULL) {
							// Parent must be a Feature
							is_geometry = 0;
						}
					}
				}
			}

			if (is_geometry) {
				json_object *jo = j;
				while (jo != NULL) {
					if (jo->parent != NULL && jo->parent->type == JSON_HASH) {
						if (json_hash_get(jo->parent, "properties") == jo) {
							// Ancestor is the value corresponding to a properties key
							is_geometry = 0;
							break;
						}
					}
					jo = jo->parent;
				}
			}

			if (is_geometry) {
				if (found_features != 0 && found_geometries == 0) {
					fprintf(stderr, "%s:%d: Warning: found a mixture of features and bare geometries\n", jfa->fname.c_str(), jp->line);
				}
				found_geometries++;

				jfa->add_feature(j, false, NULL, NULL, NULL, j);
				json_free(j);
				continue;
			}
		}

		if (strcmp(type->string, "Feature") != 0) {
			if (strcmp(type->string, "FeatureCollection") == 0) {
				jfa->check_crs(j);
				json_free(j);
			}

			continue;
		}

		if (found_features == 0 && found_geometries != 0) {
			fprintf(stderr, "%s:%d: Warning: found a mixture of features and bare geometries\n", jfa->fname.c_str(), jp->line);
		}
		found_features++;

		json_object *geometry = json_hash_get(j, "geometry");
		if (geometry == NULL) {
			fprintf(stderr, "%s:%d: feature with no geometry\n", jfa->fname.c_str(), jp->line);
			json_context(j);
			json_free(j);
			continue;
		}

		json_object *properties = json_hash_get(j, "properties");
		if (properties == NULL || (properties->type != JSON_HASH && properties->type != JSON_NULL)) {
			fprintf(stderr, "%s:%d: feature without properties hash\n", jfa->fname.c_str(), jp->line);
			json_context(j);
			json_free(j);
			continue;
		}

		bool is_feature = true;
		{
			json_object *jo = j;
			while (jo != NULL) {
				if (jo->parent != NULL && jo->parent->type == JSON_HASH) {
					if (json_hash_get(jo->parent, "properties") == jo) {
						// Ancestor is the value corresponding to a properties key
						is_feature = false;
						break;
					}
				}
				jo = jo->parent;
			}
		}
		if (!is_feature) {
			continue;
		}

		json_object *tippecanoe = json_hash_get(j, "tippecanoe");
		json_object *id = json_hash_get(j, "id");

		json_object *geometries = json_hash_get(geometry, "geometries");
		if (geometries != NULL && geometries->type == JSON_ARRAY) {
			jfa->add_feature(geometries, true, properties, id, tippecanoe, j);
		} else {
			jfa->add_feature(geometry, false, properties, id, tippecanoe, j);
		}

		json_free(j);

		/* XXX check for any non-features in the outer object */
	}
}
