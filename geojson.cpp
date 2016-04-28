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
#include <set>
#include <string>

extern "C" {
#include "jsonpull/jsonpull.h"
}

#include "pool.hpp"
#include "projection.hpp"
#include "version.hpp"
#include "memfile.hpp"
#include "serial.hpp"
#include "main.hpp"
#include "mbtiles.hpp"
#include "geojson.hpp"
#include "geometry.hpp"

#define GEOM_POINT 0	   /* array of positions */
#define GEOM_MULTIPOINT 1      /* array of arrays of positions */
#define GEOM_LINESTRING 2      /* array of arrays of positions */
#define GEOM_MULTILINESTRING 3 /* array of arrays of arrays of positions */
#define GEOM_POLYGON 4	 /* array of arrays of arrays of positions */
#define GEOM_MULTIPOLYGON 5    /* array of arrays of arrays of arrays of positions */
#define GEOM_TYPES 6

static const char *geometry_names[GEOM_TYPES] = {
	"Point", "MultiPoint", "LineString", "MultiLineString", "Polygon", "MultiPolygon",
};

static int geometry_within[GEOM_TYPES] = {
	-1,		 /* point */
	GEOM_POINT,      /* multipoint */
	GEOM_POINT,      /* linestring */
	GEOM_LINESTRING, /* multilinestring */
	GEOM_LINESTRING, /* polygon */
	GEOM_POLYGON,    /* multipolygon */
};

static int mb_geometry[GEOM_TYPES] = {
	VT_POINT, VT_POINT, VT_LINE, VT_LINE, VT_POLYGON, VT_POLYGON,
};

void parse_geometry(int t, json_object *j, long long *bbox, long long *fpos, FILE *out, int op, const char *fname, int line, long long *wx, long long *wy, int *initialized, unsigned *initial_x, unsigned *initial_y) {
	if (j == NULL || j->type != JSON_ARRAY) {
		fprintf(stderr, "%s:%d: expected array for type %d\n", fname, line, t);
		return;
	}

	int within = geometry_within[t];
	if (within >= 0) {
		size_t i;
		for (i = 0; i < j->length; i++) {
			if (within == GEOM_POINT) {
				if (i == 0 || mb_geometry[t] == GEOM_MULTIPOINT) {
					op = VT_MOVETO;
				} else {
					op = VT_LINETO;
				}
			}

			parse_geometry(within, j->array[i], bbox, fpos, out, op, fname, line, wx, wy, initialized, initial_x, initial_y);
		}
	} else {
		if (j->length >= 2 && j->array[0]->type == JSON_NUMBER && j->array[1]->type == JSON_NUMBER) {
			long long x, y;
			double lon = j->array[0]->number;
			double lat = j->array[1]->number;
			latlon2tile(lat, lon, 32, &x, &y);

			if (j->length > 2) {
				static int warned = 0;

				if (!warned) {
					fprintf(stderr, "%s:%d: ignoring dimensions beyond two\n", fname, line);
					warned = 1;
				}
			}

			if (bbox != NULL) {
				if (x < bbox[0]) {
					bbox[0] = x;
				}
				if (y < bbox[1]) {
					bbox[1] = y;
				}
				if (x > bbox[2]) {
					bbox[2] = x;
				}
				if (y > bbox[3]) {
					bbox[3] = y;
				}
			}

			if (!*initialized) {
				if (x < 0 || x >= (1LL << 32) || y < 0 || y >= (1LL < 32)) {
					*initial_x = 1LL << 31;
					*initial_y = 1LL << 31;
					*wx = 1LL << 31;
					*wy = 1LL << 31;
				} else {
					*initial_x = (x >> geometry_scale) << geometry_scale;
					*initial_y = (y >> geometry_scale) << geometry_scale;
					*wx = x;
					*wy = y;
				}

				*initialized = 1;
			}

			serialize_byte(out, op, fpos, fname);
			serialize_long_long(out, (x >> geometry_scale) - (*wx >> geometry_scale), fpos, fname);
			serialize_long_long(out, (y >> geometry_scale) - (*wy >> geometry_scale), fpos, fname);
			*wx = x;
			*wy = y;
		} else {
			fprintf(stderr, "%s:%d: malformed point\n", fname, line);
		}
	}

	if (t == GEOM_POLYGON) {
		// Note that this is not using the correct meaning of closepath.
		//
		// We are using it here to close an entire Polygon, to distinguish
		// the Polygons within a MultiPolygon from each other.
		//
		// This will be undone in fix_polygon(), which needs to know which
		// rings come from which Polygons so that it can make the winding order
		// of the outer ring be the opposite of the order of the inner rings.

		serialize_byte(out, VT_CLOSEPATH, fpos, fname);
	}
}

int serialize_geometry(json_object *geometry, json_object *properties, const char *reading, int line, volatile long long *layer_seq, volatile long long *progress_seq, long long *metapos, long long *geompos, long long *indexpos, std::set<std::string> *exclude, std::set<std::string> *include, int exclude_all, FILE *metafile, FILE *geomfile, FILE *indexfile, struct memfile *poolfile, struct memfile *treefile, const char *fname, int basezoom, int layer, double droprate, long long *file_bbox, json_object *tippecanoe, int segment, int *initialized, unsigned *initial_x, unsigned *initial_y, struct reader *readers, std::set<type_and_string> *file_keys) {
	json_object *geometry_type = json_hash_get(geometry, "type");
	if (geometry_type == NULL) {
		static int warned = 0;
		if (!warned) {
			fprintf(stderr, "%s:%d: null geometry (additional not reported)\n", reading, line);
			warned = 1;
		}

		return 0;
	}

	if (geometry_type->type != JSON_STRING) {
		fprintf(stderr, "%s:%d: geometry without type\n", reading, line);
		return 0;
	}

	json_object *coordinates = json_hash_get(geometry, "coordinates");
	if (coordinates == NULL || coordinates->type != JSON_ARRAY) {
		fprintf(stderr, "%s:%d: feature without coordinates array\n", reading, line);
		return 0;
	}

	int t;
	for (t = 0; t < GEOM_TYPES; t++) {
		if (strcmp(geometry_type->string, geometry_names[t]) == 0) {
			break;
		}
	}
	if (t >= GEOM_TYPES) {
		fprintf(stderr, "%s:%d: Can't handle geometry type %s\n", reading, line, geometry_type->string);
		return 0;
	}

	int tippecanoe_minzoom = -1;
	int tippecanoe_maxzoom = -1;

	if (tippecanoe != NULL) {
		json_object *min = json_hash_get(tippecanoe, "minzoom");
		if (min != NULL && min->type == JSON_NUMBER) {
			tippecanoe_minzoom = min->number;
		}
		if (min != NULL && min->type == JSON_STRING) {
			tippecanoe_minzoom = atoi(min->string);
		}

		json_object *max = json_hash_get(tippecanoe, "maxzoom");
		if (max != NULL && max->type == JSON_NUMBER) {
			tippecanoe_maxzoom = max->number;
		}
		if (max != NULL && max->type == JSON_STRING) {
			tippecanoe_maxzoom = atoi(max->string);
		}
	}

	long long bbox[] = {UINT_MAX, UINT_MAX, 0, 0};

	int nprop = 0;
	if (properties != NULL && properties->type == JSON_HASH) {
		nprop = properties->length;
	}

	long long metastart = *metapos;
	char *metakey[nprop];
	const char *metaval[nprop];
	int metatype[nprop];
	int mustfree[nprop];
	int m = 0;

	int i;
	for (i = 0; i < nprop; i++) {
		if (properties->keys[i]->type == JSON_STRING) {
			std::string s(properties->keys[i]->string);

			if (exclude_all) {
				if (include->count(s) == 0) {
					continue;
				}
			} else if (exclude->count(s) != 0) {
				continue;
			}

			type_and_string tas;
			tas.string = s;
			tas.type = -1;

			metakey[m] = properties->keys[i]->string;
			mustfree[m] = 0;

			if (properties->values[i] != NULL && properties->values[i]->type == JSON_STRING) {
				tas.type = metatype[m] = VT_STRING;
				metaval[m] = properties->values[i]->string;
				m++;
			} else if (properties->values[i] != NULL && properties->values[i]->type == JSON_NUMBER) {
				tas.type = metatype[m] = VT_NUMBER;
				metaval[m] = properties->values[i]->string;
				m++;
			} else if (properties->values[i] != NULL && (properties->values[i]->type == JSON_TRUE || properties->values[i]->type == JSON_FALSE)) {
				tas.type = metatype[m] = VT_BOOLEAN;
				metaval[m] = properties->values[i]->type == JSON_TRUE ? "true" : "false";
				m++;
			} else if (properties->values[i] != NULL && (properties->values[i]->type == JSON_NULL)) {
				;
			} else {
				tas.type = metatype[m] = VT_STRING;
				metaval[m] = json_stringify(properties->values[i]);
				mustfree[m] = 1;
				m++;
			}

			if (tas.type >= 0) {
				file_keys->insert(tas);
			}
		}
	}

	for (i = 0; i < m; i++) {
		serialize_long_long(metafile, addpool(poolfile, treefile, metakey[i], VT_STRING), metapos, fname);
		serialize_long_long(metafile, addpool(poolfile, treefile, metaval[i], metatype[i]), metapos, fname);

		if (mustfree[i]) {
			free((void *) metaval[i]);
		}
	}

	long long geomstart = *geompos;

	serialize_byte(geomfile, mb_geometry[t], geompos, fname);
	serialize_long_long(geomfile, *layer_seq, geompos, fname);

	serialize_long_long(geomfile, (layer << 2) | ((tippecanoe_minzoom != -1) << 1) | (tippecanoe_maxzoom != -1), geompos, fname);
	if (tippecanoe_minzoom != -1) {
		serialize_int(geomfile, tippecanoe_minzoom, geompos, fname);
	}
	if (tippecanoe_maxzoom != -1) {
		serialize_int(geomfile, tippecanoe_maxzoom, geompos, fname);
	}

	serialize_int(geomfile, segment, geompos, fname);
	serialize_long_long(geomfile, metastart, geompos, fname);
	serialize_int(geomfile, m, geompos, fname);
	long long wx = *initial_x, wy = *initial_y;
	parse_geometry(t, coordinates, bbox, geompos, geomfile, VT_MOVETO, fname, line, &wx, &wy, initialized, initial_x, initial_y);
	serialize_byte(geomfile, VT_END, geompos, fname);

	/*
	 * Note that feature_minzoom for lines is the dimension
	 * of the geometry in world coordinates, but
	 * for points is the lowest zoom level (in tiles,
	 * not in pixels) at which it should be drawn.
	 *
	 * So a line that is too small for, say, z8
	 * will have feature_minzoom of 18 (if tile detail is 10),
	 * not 8.
	 */
	int feature_minzoom = 0;
	if (mb_geometry[t] == VT_LINE) {
		// Skip z0 check because everything is always in the one z0 tile
		for (feature_minzoom = 1; feature_minzoom < 31; feature_minzoom++) {
			unsigned mask = 1 << (32 - (feature_minzoom + 1));

			if (((bbox[0] & mask) != (bbox[2] & mask)) || ((bbox[1] & mask) != (bbox[3] & mask))) {
				break;
			}
		}
	} else if (mb_geometry[t] == VT_POINT) {
		double r = ((double) rand()) / RAND_MAX;
		if (r == 0) {
			r = .00000001;
		}
		feature_minzoom = basezoom - floor(log(r) / -log(droprate));
	}

	serialize_byte(geomfile, feature_minzoom, geompos, fname);

	struct index index;
	index.start = geomstart;
	index.end = *geompos;
	index.segment = segment;
	index.seq = *layer_seq;

	// Calculate the center even if off the edge of the plane,
	// and then mask to bring it back into the addressable area
	long long midx = (bbox[0] / 2 + bbox[2] / 2) & ((1LL << 32) - 1);
	long long midy = (bbox[1] / 2 + bbox[3] / 2) & ((1LL << 32) - 1);
	index.index = encode(midx, midy);

	fwrite_check(&index, sizeof(struct index), 1, indexfile, fname);
	*indexpos += sizeof(struct index);

	for (i = 0; i < 2; i++) {
		if (bbox[i] < file_bbox[i]) {
			file_bbox[i] = bbox[i];
		}
	}
	for (i = 2; i < 4; i++) {
		if (bbox[i] > file_bbox[i]) {
			file_bbox[i] = bbox[i];
		}
	}

	if (*progress_seq % 10000 == 0) {
		checkdisk(readers, CPUS);
		if (!quiet) {
			fprintf(stderr, "Read %.2f million features\r", *progress_seq / 1000000.0);
		}
	}
	(*progress_seq)++;
	(*layer_seq)++;

	return 1;
}

void parse_json(json_pull *jp, const char *reading, volatile long long *layer_seq, volatile long long *progress_seq, long long *metapos, long long *geompos, long long *indexpos, std::set<std::string> *exclude, std::set<std::string> *include, int exclude_all, FILE *metafile, FILE *geomfile, FILE *indexfile, struct memfile *poolfile, struct memfile *treefile, char *fname, int basezoom, int layer, double droprate, long long *file_bbox, int segment, int *initialized, unsigned *initial_x, unsigned *initial_y, struct reader *readers, std::set<type_and_string> *file_keys) {
	long long found_hashes = 0;
	long long found_features = 0;
	long long found_geometries = 0;

	while (1) {
		json_object *j = json_read(jp);
		if (j == NULL) {
			if (jp->error != NULL) {
				fprintf(stderr, "%s:%d: %s\n", reading, jp->line, jp->error);
			}

			json_free(jp->root);
			break;
		}

		if (j->type == JSON_HASH) {
			found_hashes++;

			if (found_hashes == 50 && found_features == 0 && found_geometries == 0) {
				fprintf(stderr, "%s:%d: Warning: not finding any GeoJSON features or geometries in input yet after 50 objects.\n", reading, jp->line);
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
					if (j->parent->type == JSON_ARRAY) {
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
					fprintf(stderr, "%s:%d: Warning: found a mixture of features and bare geometries\n", reading, jp->line);
				}
				found_geometries++;

				serialize_geometry(j, NULL, reading, jp->line, layer_seq, progress_seq, metapos, geompos, indexpos, exclude, include, exclude_all, metafile, geomfile, indexfile, poolfile, treefile, fname, basezoom, layer, droprate, file_bbox, NULL, segment, initialized, initial_x, initial_y, readers, file_keys);
				json_free(j);
				continue;
			}
		}

		if (strcmp(type->string, "Feature") != 0) {
			continue;
		}

		if (found_features == 0 && found_geometries != 0) {
			fprintf(stderr, "%s:%d: Warning: found a mixture of features and bare geometries\n", reading, jp->line);
		}
		found_features++;

		json_object *geometry = json_hash_get(j, "geometry");
		if (geometry == NULL) {
			fprintf(stderr, "%s:%d: feature with no geometry\n", reading, jp->line);
			json_free(j);
			continue;
		}

		json_object *properties = json_hash_get(j, "properties");
		if (properties == NULL || (properties->type != JSON_HASH && properties->type != JSON_NULL)) {
			fprintf(stderr, "%s:%d: feature without properties hash\n", reading, jp->line);
			json_free(j);
			continue;
		}

		json_object *tippecanoe = json_hash_get(j, "tippecanoe");

		json_object *geometries = json_hash_get(geometry, "geometries");
		if (geometries != NULL) {
			size_t g;
			for (g = 0; g < geometries->length; g++) {
				serialize_geometry(geometries->array[g], properties, reading, jp->line, layer_seq, progress_seq, metapos, geompos, indexpos, exclude, include, exclude_all, metafile, geomfile, indexfile, poolfile, treefile, fname, basezoom, layer, droprate, file_bbox, tippecanoe, segment, initialized, initial_x, initial_y, readers, file_keys);
			}
		} else {
			serialize_geometry(geometry, properties, reading, jp->line, layer_seq, progress_seq, metapos, geompos, indexpos, exclude, include, exclude_all, metafile, geomfile, indexfile, poolfile, treefile, fname, basezoom, layer, droprate, file_bbox, tippecanoe, segment, initialized, initial_x, initial_y, readers, file_keys);
		}

		json_free(j);

		/* XXX check for any non-features in the outer object */
	}
}

void *run_parse_json(void *v) {
	struct parse_json_args *pja = (struct parse_json_args *) v;

	parse_json(pja->jp, pja->reading, pja->layer_seq, pja->progress_seq, pja->metapos, pja->geompos, pja->indexpos, pja->exclude, pja->include, pja->exclude_all, pja->metafile, pja->geomfile, pja->indexfile, pja->poolfile, pja->treefile, pja->fname, pja->basezoom, pja->layer, pja->droprate, pja->file_bbox, pja->segment, pja->initialized, pja->initial_x, pja->initial_y, pja->readers, pja->file_keys);

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
	struct jsonmap *jm = (struct jsonmap *) malloc(sizeof(struct jsonmap));
	if (jm == NULL) {
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}

	jm->map = map;
	jm->off = 0;
	jm->end = len;

	return json_begin(json_map_read, jm);
}
