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
#include "geojson-loop.hpp"

int serialize_geojson_feature(struct serialization_state *sst, json_object *geometry, json_object *properties, json_object *id, int layer, json_object *tippecanoe, json_object *feature, std::string layername) {
	json_object *geometry_type = json_hash_get(geometry, "type");
	if (geometry_type == NULL) {
		static int warned = 0;
		if (!warned) {
			fprintf(stderr, "%s:%d: null geometry (additional not reported)\n", sst->fname, sst->line);
			json_context(feature);
			warned = 1;
		}

		return 0;
	}

	if (geometry_type->type != JSON_STRING) {
		fprintf(stderr, "%s:%d: geometry type is not a string\n", sst->fname, sst->line);
		json_context(feature);
		return 0;
	}

	json_object *coordinates = json_hash_get(geometry, "coordinates");
	if (coordinates == NULL || coordinates->type != JSON_ARRAY) {
		fprintf(stderr, "%s:%d: feature without coordinates array\n", sst->fname, sst->line);
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
		fprintf(stderr, "%s:%d: Can't handle geometry type %s\n", sst->fname, sst->line, geometry_type->string);
		json_context(feature);
		return 0;
	}

	int tippecanoe_minzoom = -1;
	int tippecanoe_maxzoom = -1;
	std::string tippecanoe_layername;

	if (tippecanoe != NULL) {
		json_object *min = json_hash_get(tippecanoe, "minzoom");
		if (min != NULL && (min->type == JSON_STRING || min->type == JSON_NUMBER)) {
			tippecanoe_minzoom = integer_zoom(sst->fname, min->string);
		}

		json_object *max = json_hash_get(tippecanoe, "maxzoom");
		if (max != NULL && (max->type == JSON_STRING || max->type == JSON_NUMBER)) {
			tippecanoe_maxzoom = integer_zoom(sst->fname, max->string);
		}

		json_object *ln = json_hash_get(tippecanoe, "layer");
		if (ln != NULL && (ln->type == JSON_STRING || ln->type == JSON_NUMBER)) {
			tippecanoe_layername = std::string(ln->string);
		}
	}

	bool has_id = false;
	unsigned long long id_value = 0;
	if (id != NULL) {
		if (id->type == JSON_NUMBER) {
			if (id->number >= 0) {
				char *err = NULL;
				id_value = strtoull(id->string, &err, 10);

				if (err != NULL && *err != '\0') {
					static bool warned_frac = false;

					if (!warned_frac) {
						fprintf(stderr, "Warning: Can't represent non-integer feature ID %s\n", id->string);
						warned_frac = true;
					}
				} else if (std::to_string(id_value) != id->string) {
					static bool warned = false;

					if (!warned) {
						fprintf(stderr, "Warning: Can't represent too-large feature ID %s\n", id->string);
						warned = true;
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
			bool converted = false;

			if (additional[A_CONVERT_NUMERIC_IDS] && id->type == JSON_STRING) {
				char *err = NULL;
				id_value = strtoull(id->string, &err, 10);

				if (err != NULL && *err != '\0') {
					static bool warned_frac = false;

					if (!warned_frac) {
						fprintf(stderr, "Warning: Can't represent non-integer feature ID %s\n", id->string);
						warned_frac = true;
					}
				} else if (std::to_string(id_value) != id->string) {
					static bool warned = false;

					if (!warned) {
						fprintf(stderr, "Warning: Can't represent too-large feature ID %s\n", id->string);
						warned = true;
					}
				} else {
					has_id = true;
					converted = true;
				}
			}

			if (!converted) {
				static bool warned_nan = false;

				if (!warned_nan) {
					char *s = json_stringify(id);
					fprintf(stderr, "Warning: Can't represent non-numeric feature ID %s\n", s);
					free(s);  // stringify
					warned_nan = true;
				}
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
			if (name != NULL && name->type == JSON_STRING) {
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

struct json_serialize_action : json_feature_action {
	serialization_state *sst;
	int layer;
	std::string layername;

	int add_feature(json_object *geometry, bool geometrycollection, json_object *properties, json_object *id, json_object *tippecanoe, json_object *feature) {
		sst->line = geometry->parser->line;
		if (geometrycollection) {
			int ret = 1;
			for (size_t g = 0; g < geometry->length; g++) {
				ret &= serialize_geojson_feature(sst, geometry->array[g], properties, id, layer, tippecanoe, feature, layername);
			}
			return ret;
		} else {
			return serialize_geojson_feature(sst, geometry, properties, id, layer, tippecanoe, feature, layername);
		}
	}

	void check_crs(json_object *j) {
		::check_crs(j, fname.c_str());
	}
};

void parse_json(struct serialization_state *sst, json_pull *jp, int layer, std::string layername) {
	json_serialize_action jsa;
	jsa.fname = sst->fname;
	jsa.sst = sst;
	jsa.layer = layer;
	jsa.layername = layername;

	parse_json(&jsa, jp);
}

void *run_parse_json(void *v) {
	struct parse_json_args *pja = (struct parse_json_args *) v;

	parse_json(pja->sst, pja->jp, pja->layer, *pja->layername);

	return NULL;
}

struct jsonmap {
	char *map;
	unsigned long long off;
	unsigned long long end;
};

ssize_t json_map_read(struct json_pull *jp, char *buffer, size_t n) {
	struct jsonmap *jm = (struct jsonmap *) jp->source;

	if (jm->off + n >= jm->end) {
		n = jm->end - jm->off;
	}

	memcpy(buffer, jm->map + jm->off, n);
	jm->off += n;

	return n;
}

struct json_pull *json_begin_map(char *map, long long len) {
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
