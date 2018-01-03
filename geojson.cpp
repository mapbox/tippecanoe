#ifdef MTRACE
#include <mcheck.h>
#endif

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
#include <limits.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <sys/resource.h>
#include <pthread.h>
#include <vector>
#include <algorithm>
#include <set>
#include <map>
#include <string>
#include "jsonpull/jsonpull.h"
#include "pool.hpp"
#include "projection.hpp"
#include "memfile.hpp"
#include "main.hpp"
#include "mbtiles.hpp"
#include "geojson.hpp"
#include "geometry.hpp"
#include "options.hpp"
#include "serial.hpp"
#include "text.hpp"
#include "read_json.hpp"
#include "mvt.hpp"

bool serialize_geojson_feature(struct serialization_state *sst, json_object *geometry, json_object *properties, json_object *id, size_t layer, json_object *tippecanoe, json_object *feature, std::string layername) {
	json_object *geometry_type = json_hash_get(geometry, "type");
	if (geometry_type == NULL) {
		static bool warned = 0;
		if (!warned) {
			fprintf(stderr, "%s:%zu: null geometry (additional not reported)\n", sst->fname, sst->line);
			json_context(feature);
			warned = 1;
		}

		return 0;
	}

	if (geometry_type->type != JSON_STRING) {
		fprintf(stderr, "%s:%zu: geometry type is not a string\n", sst->fname, sst->line);
		json_context(feature);
		return 0;
	}

	json_object *coordinates = json_hash_get(geometry, "coordinates");
	if (coordinates == NULL || coordinates->type != JSON_ARRAY) {
		fprintf(stderr, "%s:%zu: feature without coordinates array\n", sst->fname, sst->line);
		json_context(feature);
		return 0;
	}

	int t;
	for (t = 0; t < GEOM_TYPES; t++) {
		if (strcmp(geometry_type->string, geometry_names[t]) == 0) {
			break;
		}
	}
	if (t >= GEOM_TYPES) {
		fprintf(stderr, "%s:%zu: Can't handle geometry type %s\n", sst->fname, sst->line, geometry_type->string);
		json_context(feature);
		return 0;
	}

	int tippecanoe_minzoom = -1;
	int tippecanoe_maxzoom = -1;
	std::string tippecanoe_layername;

	if (tippecanoe != NULL) {
		json_object *min = json_hash_get(tippecanoe, "minzoom");
		if (min != NULL && min->type == JSON_NUMBER) {
			tippecanoe_minzoom = (int) min->number;
		}
		if (min != NULL && min->type == JSON_STRING) {
			tippecanoe_minzoom = atoi(min->string);
		}

		json_object *max = json_hash_get(tippecanoe, "maxzoom");
		if (max != NULL && max->type == JSON_NUMBER) {
			tippecanoe_maxzoom = (int) max->number;
		}
		if (max != NULL && max->type == JSON_STRING) {
			tippecanoe_maxzoom = atoi(max->string);
		}

		json_object *ln = json_hash_get(tippecanoe, "layer");
		if (ln != NULL && (ln->type == JSON_STRING || ln->type == JSON_NUMBER)) {
			tippecanoe_layername = std::string(ln->string);
		}
	}

	bool has_id = false;
	unsigned long id_value = 0;
	if (id != NULL) {
		if (id->type == JSON_NUMBER) {
			if (id->number >= 0) {
				char *err = NULL;
				id_value = strtoul(id->string, &err, 10);

				if (err != NULL && *err != '\0') {
					static bool warned_frac = false;

					if (!warned_frac) {
						fprintf(stderr, "Warning: Can't represent non-integer feature ID %s\n", id->string);
						warned_frac = true;
					}
				} else {
					has_id = true;
				}
			} else {
				static bool warned_neg = false;

				if (!warned_neg) {
					fprintf(stderr, "Warning: Can't represent negative feature ID %s\n", id->string);
					warned_neg = true;
				}
			}
		} else {
			static bool warned_nan = false;

			if (!warned_nan) {
				char *s = json_stringify(id);
				fprintf(stderr, "Warning: Can't represent non-numeric feature ID %s\n", s);
				free(s);  // stringify
				warned_nan = true;
			}
		}
	}

	size_t nprop = 0;
	if (properties != NULL && properties->type == JSON_HASH) {
		nprop = properties->length;
	}

	std::vector<char *> metakey;
	metakey.resize(nprop);

	std::vector<std::string> metaval;
	metaval.resize(nprop);

	std::vector<int> metatype;
	metatype.resize(nprop);

	size_t m = 0;

	for (size_t i = 0; i < nprop; i++) {
		if (properties->keys[i]->type == JSON_STRING) {
			std::string s(properties->keys[i]->string);

			int type = -1;
			std::string val;
			stringify_value(properties->values[i], type, val, sst->fname, sst->line, feature);

			if (type >= 0) {
				metakey[m] = properties->keys[i]->string;
				metatype[m] = type;
				metaval[m] = val;
				m++;
			} else {
				metakey[m] = properties->keys[i]->string;
				metatype[m] = mvt_null;
				metaval[m] = "null";
				m++;
			}
		}
	}

	drawvec dv;
	parse_geometry(t, coordinates, dv, VT_MOVETO, sst->fname, sst->line, feature);

	serial_feature sf;
	sf.layer = layer;
	sf.segment = sst->segment;
	sf.t = mb_geometry[t];
	sf.has_id = has_id;
	sf.id = id_value;
	sf.has_tippecanoe_minzoom = (tippecanoe_minzoom != -1);
	sf.tippecanoe_minzoom = tippecanoe_minzoom;
	sf.has_tippecanoe_maxzoom = (tippecanoe_maxzoom != -1);
	sf.tippecanoe_maxzoom = tippecanoe_maxzoom;
	sf.geometry = dv;
	sf.m = m;
	sf.feature_minzoom = 0;  // Will be filled in during index merging
	sf.seq = *(sst->layer_seq);

	if (tippecanoe_layername.size() != 0) {
		sf.layername = tippecanoe_layername;
	} else {
		sf.layername = layername;
	}

	for (size_t i = 0; i < m; i++) {
		sf.full_keys.push_back(metakey[i]);

		serial_val sv;
		sv.type = metatype[i];
		sv.s = metaval[i];

		sf.full_values.push_back(sv);
	}

	return serialize_feature(sst, sf);
}

void check_crs(json_object *j, const char *reading) {
	json_object *crs = json_hash_get(j, "crs");
	if (crs != NULL) {
		json_object *properties = json_hash_get(crs, "properties");
		if (properties != NULL) {
			json_object *name = json_hash_get(properties, "name");
			if (name->type == JSON_STRING) {
				if (strcmp(name->string, projection->alias) != 0) {
					if (!quiet) {
						fprintf(stderr, "%s: Warning: GeoJSON specified projection \"%s\", not the expected \"%s\".\n", reading, name->string, projection->alias);
						fprintf(stderr, "%s: If \"%s\" is not the expected projection, use -s to specify the right one.\n", reading, projection->alias);
					}
				}
			}
		}
	}
}

void parse_json(struct serialization_state *sst, json_pull *jp, size_t layer, std::string layername) {
	size_t found_hashes = 0;
	size_t found_features = 0;
	size_t found_geometries = 0;

	while (1) {
		json_object *j = json_read(jp);
		if (j == NULL) {
			if (jp->error != NULL) {
				fprintf(stderr, "%s:%zu: %s\n", sst->fname, jp->line, jp->error);
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
				fprintf(stderr, "%s:%zu: Warning: not finding any GeoJSON features or geometries in input yet after 50 objects.\n", sst->fname, jp->line);
			}
		}

		json_object *type = json_hash_get(j, "type");
		if (type == NULL || type->type != JSON_STRING) {
			continue;
		}

		if (found_features == 0) {
			bool is_geometry = 0;
			for (size_t i = 0; i < GEOM_TYPES; i++) {
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
				if (found_features != 0 && found_geometries == 0) {
					fprintf(stderr, "%s:%zu: Warning: found a mixture of features and bare geometries\n", sst->fname, jp->line);
				}
				found_geometries++;

				serialize_geojson_feature(sst, j, NULL, NULL, layer, NULL, j, layername);
				json_free(j);
				continue;
			}
		}

		if (strcmp(type->string, "Feature") != 0) {
			if (strcmp(type->string, "FeatureCollection") == 0) {
				check_crs(j, sst->fname);
				json_free(j);
			}

			continue;
		}

		if (found_features == 0 && found_geometries != 0) {
			fprintf(stderr, "%s:%zu: Warning: found a mixture of features and bare geometries\n", sst->fname, jp->line);
		}
		found_features++;

		json_object *geometry = json_hash_get(j, "geometry");
		if (geometry == NULL) {
			fprintf(stderr, "%s:%zu: feature with no geometry\n", sst->fname, jp->line);
			json_context(j);
			json_free(j);
			continue;
		}

		json_object *properties = json_hash_get(j, "properties");
		if (properties == NULL || (properties->type != JSON_HASH && properties->type != JSON_NULL)) {
			fprintf(stderr, "%s:%zu: feature without properties hash\n", sst->fname, jp->line);
			json_context(j);
			json_free(j);
			continue;
		}

		json_object *tippecanoe = json_hash_get(j, "tippecanoe");
		json_object *id = json_hash_get(j, "id");

		json_object *geometries = json_hash_get(geometry, "geometries");
		if (geometries != NULL) {
			size_t g;
			for (g = 0; g < geometries->length; g++) {
				serialize_geojson_feature(sst, geometries->array[g], properties, id, layer, tippecanoe, j, layername);
			}
		} else {
			serialize_geojson_feature(sst, geometry, properties, id, layer, tippecanoe, j, layername);
		}

		json_free(j);

		/* XXX check for any non-features in the outer object */
	}
}

void *run_parse_json(void *v) {
	struct parse_json_args *pja = (struct parse_json_args *) v;

	parse_json(pja->sst, pja->jp, pja->layer, *pja->layername);

	return NULL;
}

struct jsonmap {
	char *map;
	size_t off;
	size_t end;
};

ssize_t json_map_read(struct json_pull *jp, char *buffer, size_t n) {
	struct jsonmap *jm = (struct jsonmap *) jp->source;

	if (jm->off + n >= jm->end) {
		n = jm->end - jm->off;
	}

	memcpy(buffer, jm->map + jm->off, n);
	jm->off += n;

	return (ssize_t) n;
}

struct json_pull *json_begin_map(char *map, size_t len) {
	struct jsonmap *jm = new jsonmap;
	if (jm == NULL) {
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}

	jm->map = map;
	jm->off = 0;
	jm->end = len;

	return json_begin(json_map_read, jm);
}

void json_end_map(struct json_pull *jp) {
	delete (struct jsonmap *) jp->source;
	json_end(jp);
}
