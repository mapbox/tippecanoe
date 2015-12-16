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

#include "jsonpull.h"
#include "tile.h"
#include "pool.h"
#include "mbtiles.h"
#include "projection.h"
#include "version.h"
#include "memfile.h"

int low_detail = 12;
int full_detail = -1;
int min_detail = 7;
int quiet = 0;

unsigned initial_x = 0, initial_y = 0;
int geometry_scale = 0;
int initialized = 0;

#define GEOM_POINT 0	   /* array of positions */
#define GEOM_MULTIPOINT 1      /* array of arrays of positions */
#define GEOM_LINESTRING 2      /* array of arrays of positions */
#define GEOM_MULTILINESTRING 3 /* array of arrays of arrays of positions */
#define GEOM_POLYGON 4	 /* array of arrays of arrays of positions */
#define GEOM_MULTIPOLYGON 5    /* array of arrays of arrays of arrays of positions */
#define GEOM_TYPES 6

const char *geometry_names[GEOM_TYPES] = {
	"Point", "MultiPoint", "LineString", "MultiLineString", "Polygon", "MultiPolygon",
};

int geometry_within[GEOM_TYPES] = {
	-1,		 /* point */
	GEOM_POINT,      /* multipoint */
	GEOM_POINT,      /* linestring */
	GEOM_LINESTRING, /* multilinestring */
	GEOM_LINESTRING, /* polygon */
	GEOM_POLYGON,    /* multipolygon */
};

int mb_geometry[GEOM_TYPES] = {
	VT_POINT, VT_POINT, VT_LINE, VT_LINE, VT_POLYGON, VT_POLYGON,
};

int CPUS;
int TEMP_FILES;

void init_cpus() {
	CPUS = sysconf(_SC_NPROCESSORS_ONLN);
	if (CPUS < 1) {
		CPUS = 1;
	}

	TEMP_FILES = 64;
	struct rlimit rl;
	if (getrlimit(RLIMIT_NOFILE, &rl) != 0) {
		perror("getrlimit");
	} else {
		TEMP_FILES = rl.rlim_cur / 3;
		if (TEMP_FILES > CPUS * 4) {
			TEMP_FILES = CPUS * 4;
		}
	}
}

size_t fwrite_check(const void *ptr, size_t size, size_t nitems, FILE *stream, const char *fname) {
	size_t w = fwrite(ptr, size, nitems, stream);
	if (w != nitems) {
		fprintf(stderr, "%s: Write to temporary file failed: %s\n", fname, strerror(errno));
		exit(EXIT_FAILURE);
	}
	return w;
}

void serialize_int(FILE *out, int n, long long *fpos, const char *fname) {
	serialize_long_long(out, n, fpos, fname);
}

void serialize_long_long(FILE *out, long long n, long long *fpos, const char *fname) {
	unsigned long long zigzag = (n << 1) ^ (n >> 63);

	while (1) {
		unsigned char b = zigzag & 0x7F;
		if ((zigzag >> 7) != 0) {
			b |= 0x80;
			if (putc(b, out) == EOF) {
				fprintf(stderr, "%s: Write to temporary file failed: %s\n", fname, strerror(errno));
				exit(EXIT_FAILURE);
			}
			*fpos += 1;
			zigzag >>= 7;
		} else {
			if (putc(b, out) == EOF) {
				fprintf(stderr, "%s: Write to temporary file failed: %s\n", fname, strerror(errno));
				exit(EXIT_FAILURE);
			}
			*fpos += 1;
			break;
		}
	}
}

void serialize_byte(FILE *out, signed char n, long long *fpos, const char *fname) {
	fwrite_check(&n, sizeof(signed char), 1, out, fname);
	*fpos += sizeof(signed char);
}

void serialize_uint(FILE *out, unsigned n, long long *fpos, const char *fname) {
	fwrite_check(&n, sizeof(unsigned), 1, out, fname);
	*fpos += sizeof(unsigned);
}

void serialize_string(FILE *out, const char *s, long long *fpos, const char *fname) {
	int len = strlen(s);

	serialize_int(out, len + 1, fpos, fname);
	fwrite_check(s, sizeof(char), len, out, fname);
	fwrite_check("", sizeof(char), 1, out, fname);
	*fpos += len + 1;
}

void parse_geometry(int t, json_object *j, long long *bbox, long long *fpos, FILE *out, int op, const char *fname, json_pull *source, long long *wx, long long *wy, int *initialized) {
	if (j == NULL || j->type != JSON_ARRAY) {
		fprintf(stderr, "%s:%d: expected array for type %d\n", fname, source->line, t);
		return;
	}

	int within = geometry_within[t];
	if (within >= 0) {
		int i;
		for (i = 0; i < j->length; i++) {
			if (within == GEOM_POINT) {
				if (i == 0 || mb_geometry[t] == GEOM_MULTIPOINT) {
					op = VT_MOVETO;
				} else {
					op = VT_LINETO;
				}
			}

			parse_geometry(within, j->array[i], bbox, fpos, out, op, fname, source, wx, wy, initialized);
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
					fprintf(stderr, "%s:%d: ignoring dimensions beyond two\n", fname, source->line);
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
				initial_x = x;
				initial_y = y;
				*wx = x;
				*wy = y;
				*initialized = 1;
			}

			serialize_byte(out, op, fpos, fname);
			serialize_long_long(out, (x >> geometry_scale) - (*wx >> geometry_scale), fpos, fname);
			serialize_long_long(out, (y >> geometry_scale) - (*wy >> geometry_scale), fpos, fname);
			*wx = x;
			*wy = y;
		} else {
			fprintf(stderr, "%s:%d: malformed point\n", fname, source->line);
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

void deserialize_int(char **f, int *n) {
	long long ll;
	deserialize_long_long(f, &ll);
	*n = ll;
}

void deserialize_long_long(char **f, long long *n) {
	unsigned long long zigzag = 0;
	int shift = 0;

	while (1) {
		if ((**f & 0x80) == 0) {
			zigzag |= ((unsigned long long) **f) << shift;
			*f += 1;
			shift += 7;
			break;
		} else {
			zigzag |= ((unsigned long long) (**f & 0x7F)) << shift;
			*f += 1;
			shift += 7;
		}
	}

	*n = (zigzag >> 1) ^ (-(zigzag & 1));
}

void deserialize_uint(char **f, unsigned *n) {
	memcpy(n, *f, sizeof(unsigned));
	*f += sizeof(unsigned);
}

void deserialize_byte(char **f, signed char *n) {
	memcpy(n, *f, sizeof(signed char));
	*f += sizeof(signed char);
}

struct pool_val *deserialize_string(char **f, struct pool *p, int type) {
	struct pool_val *ret;
	int len;

	deserialize_int(f, &len);
	ret = pool(p, *f, type);
	*f += len;

	return ret;
}

struct index {
	long long start;
	long long end;
	unsigned long long index;
};

int indexcmp(const void *v1, const void *v2) {
	const struct index *i1 = (const struct index *) v1;
	const struct index *i2 = (const struct index *) v2;

	if (i1->index < i2->index) {
		return -1;
	} else if (i1->index > i2->index) {
		return 1;
	}

	return 0;
}

struct merge {
	long long start;
	long long end;

	struct merge *next;
};

static void insert(struct merge *m, struct merge **head, unsigned char *map, int bytes) {
	while (*head != NULL && indexcmp(map + m->start, map + (*head)->start) > 0) {
		head = &((*head)->next);
	}

	m->next = *head;
	*head = m;
}

static void merge(struct merge *merges, int nmerges, unsigned char *map, FILE *f, int bytes, long long nrec) {
	int i;
	struct merge *head = NULL;
	long long along = 0;
	long long reported = -1;

	for (i = 0; i < nmerges; i++) {
		if (merges[i].start < merges[i].end) {
			insert(&(merges[i]), &head, map, bytes);
		}
	}

	while (head != NULL) {
		fwrite_check(map + head->start, bytes, 1, f, "merge temporary");
		head->start += bytes;

		struct merge *m = head;
		head = m->next;
		m->next = NULL;

		if (m->start < m->end) {
			insert(m, &head, map, bytes);
		}

		along++;
		long long report = 100 * along / nrec;
		if (report != reported) {
			if (!quiet) {
				fprintf(stderr, "Merging: %lld%%\r", report);
			}
			reported = report;
		}
	}
}

struct stringpool {
	long long left;
	long long right;
	long long off;
};
long long pooltree = 0;

static unsigned char swizzle[256] = {
	0x00, 0xBF, 0x18, 0xDE, 0x93, 0xC9, 0xB1, 0x5E, 0xDF, 0xBE, 0x72, 0x5A, 0xBB, 0x42, 0x64, 0xC6,
	0xD8, 0xB7, 0x15, 0x74, 0x1C, 0x8B, 0x91, 0xF5, 0x29, 0x46, 0xEC, 0x6F, 0xCA, 0x20, 0xF0, 0x06,
	0x27, 0x61, 0x87, 0xE0, 0x6E, 0x43, 0x50, 0xC5, 0x1B, 0xB4, 0x37, 0xC3, 0x69, 0xA6, 0xEE, 0x80,
	0xAF, 0x9B, 0xA1, 0x76, 0x23, 0x24, 0x53, 0xF3, 0x5B, 0x65, 0x19, 0xF4, 0xFC, 0xDD, 0x26, 0xE8,
	0x10, 0xF7, 0xCE, 0x92, 0x48, 0xF6, 0x94, 0x60, 0x07, 0xC4, 0xB9, 0x97, 0x6D, 0xA4, 0x11, 0x0D,
	0x1F, 0x4D, 0x13, 0xB0, 0x5D, 0xBA, 0x31, 0xD5, 0x8D, 0x51, 0x36, 0x96, 0x7A, 0x03, 0x7F, 0xDA,
	0x17, 0xDB, 0xD4, 0x83, 0xE2, 0x79, 0x6A, 0xE1, 0x95, 0x38, 0xFF, 0x28, 0xB2, 0xB3, 0xA7, 0xAE,
	0xF8, 0x54, 0xCC, 0xDC, 0x9A, 0x6B, 0xFB, 0x3F, 0xD7, 0xBC, 0x21, 0xC8, 0x71, 0x09, 0x16, 0xAC,
	0x3C, 0x8A, 0x62, 0x05, 0xC2, 0x8C, 0x32, 0x4E, 0x35, 0x9C, 0x5F, 0x75, 0xCD, 0x2E, 0xA2, 0x3E,
	0x1A, 0xC1, 0x8E, 0x14, 0xA0, 0xD3, 0x7D, 0xD9, 0xEB, 0x5C, 0x70, 0xE6, 0x9E, 0x12, 0x3B, 0xEF,
	0x1E, 0x49, 0xD2, 0x98, 0x39, 0x7E, 0x44, 0x4B, 0x6C, 0x88, 0x02, 0x2C, 0xAD, 0xE5, 0x9F, 0x40,
	0x7B, 0x4A, 0x3D, 0xA9, 0xAB, 0x0B, 0xD6, 0x2F, 0x90, 0x2A, 0xB6, 0x1D, 0xC7, 0x22, 0x55, 0x34,
	0x0A, 0xD0, 0xB5, 0x68, 0xE3, 0x59, 0xFD, 0xFA, 0x57, 0x77, 0x25, 0xA3, 0x04, 0xB8, 0x33, 0x89,
	0x78, 0x82, 0xE4, 0xC0, 0x0E, 0x8F, 0x85, 0xD1, 0x84, 0x08, 0x67, 0x47, 0x9D, 0xCB, 0x58, 0x4C,
	0xAA, 0xED, 0x52, 0xF2, 0x4F, 0xF1, 0x66, 0xCF, 0xA5, 0x56, 0xEA, 0x7C, 0xE9, 0x63, 0xE7, 0x01,
	0xF9, 0xFE, 0x0C, 0x99, 0x2D, 0x0F, 0x3A, 0x41, 0x45, 0xA8, 0x30, 0x2B, 0x73, 0xBD, 0x86, 0x81,
};

int swizzlecmp(char *a, char *b) {
	while (*a || *b) {
		int aa = swizzle[(unsigned char) *a];
		int bb = swizzle[(unsigned char) *b];

		int cmp = aa - bb;
		if (cmp != 0) {
			return cmp;
		}

		a++;
		b++;
	}

	return 0;
}

long long addpool(struct memfile *poolfile, struct memfile *treefile, char *s, char type) {
	long long *sp = &pooltree;

	while (*sp != 0) {
		int cmp = swizzlecmp(s, poolfile->map + ((struct stringpool *) (treefile->map + *sp))->off + 1);

		if (cmp == 0) {
			cmp = type - (poolfile->map + ((struct stringpool *) (treefile->map + *sp))->off)[0];
		}

		if (cmp < 0) {
			sp = &(((struct stringpool *) (treefile->map + *sp))->left);
		} else if (cmp > 0) {
			sp = &(((struct stringpool *) (treefile->map + *sp))->right);
		} else {
			return ((struct stringpool *) (treefile->map + *sp))->off;
		}
	}

	// *sp is probably in the memory-mapped file, and will move if the file grows.
	long long ssp;
	if (sp == &pooltree) {
		ssp = -1;
	} else {
		ssp = ((char *) sp) - treefile->map;
	}

	long long off = poolfile->off;
	if (memfile_write(poolfile, &type, 1) < 0) {
		perror("memfile write");
		exit(EXIT_FAILURE);
	}
	if (memfile_write(poolfile, s, strlen(s) + 1) < 0) {
		perror("memfile write");
		exit(EXIT_FAILURE);
	}

	struct stringpool tsp;
	tsp.left = 0;
	tsp.right = 0;
	tsp.off = off;

	long long p = treefile->off;
	if (memfile_write(treefile, &tsp, sizeof(struct stringpool)) < 0) {
		perror("memfile write");
		exit(EXIT_FAILURE);
	}

	if (ssp == -1) {
		pooltree = p;
	} else {
		*((long long *) (treefile->map + ssp)) = p;
	}
	return off;
}

int serialize_geometry(json_object *geometry, json_object *properties, const char *reading, json_pull *jp, long long *seq, long long *metapos, long long *geompos, long long *indexpos, struct pool *exclude, struct pool *include, int exclude_all, FILE *metafile, FILE *geomfile, FILE *indexfile, struct memfile *poolfile, struct memfile *treefile, const char *fname, int maxzoom, int basezoom, int layer, double droprate, long long *file_bbox, json_object *tippecanoe) {
	json_object *geometry_type = json_hash_get(geometry, "type");
	if (geometry_type == NULL) {
		static int warned = 0;
		if (!warned) {
			fprintf(stderr, "%s:%d: null geometry (additional not reported)\n", reading, jp->line);
			warned = 1;
		}

		return 0;
	}

	if (geometry_type->type != JSON_STRING) {
		fprintf(stderr, "%s:%d: geometry without type\n", reading, jp->line);
		return 0;
	}

	json_object *coordinates = json_hash_get(geometry, "coordinates");
	if (coordinates == NULL || coordinates->type != JSON_ARRAY) {
		fprintf(stderr, "%s:%d: feature without coordinates array\n", reading, jp->line);
		return 0;
	}

	int t;
	for (t = 0; t < GEOM_TYPES; t++) {
		if (strcmp(geometry_type->string, geometry_names[t]) == 0) {
			break;
		}
	}
	if (t >= GEOM_TYPES) {
		fprintf(stderr, "%s:%d: Can't handle geometry type %s\n", reading, jp->line, geometry_type->string);
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
	char *metaval[nprop];
	int metatype[nprop];
	int m = 0;

	int i;
	for (i = 0; i < nprop; i++) {
		if (properties->keys[i]->type == JSON_STRING) {
			if (exclude_all) {
				if (!is_pooled(include, properties->keys[i]->string, VT_STRING)) {
					continue;
				}
			} else if (is_pooled(exclude, properties->keys[i]->string, VT_STRING)) {
				continue;
			}

			metakey[m] = properties->keys[i]->string;

			if (properties->values[i] != NULL && properties->values[i]->type == JSON_STRING) {
				metatype[m] = VT_STRING;
				metaval[m] = properties->values[i]->string;
				m++;
			} else if (properties->values[i] != NULL && properties->values[i]->type == JSON_NUMBER) {
				metatype[m] = VT_NUMBER;
				metaval[m] = properties->values[i]->string;
				m++;
			} else if (properties->values[i] != NULL && (properties->values[i]->type == JSON_TRUE || properties->values[i]->type == JSON_FALSE)) {
				metatype[m] = VT_BOOLEAN;
				metaval[m] = properties->values[i]->type == JSON_TRUE ? "true" : "false";
				m++;
			} else if (properties->values[i] != NULL && (properties->values[i]->type == JSON_NULL)) {
				;
			} else {
				fprintf(stderr, "%s:%d: Unsupported property type for %s\n", reading, jp->line, properties->keys[i]->string);
				continue;
			}
		}
	}

	serialize_int(metafile, m, metapos, fname);
	for (i = 0; i < m; i++) {
		serialize_long_long(metafile, addpool(poolfile, treefile, metakey[i], VT_STRING), metapos, fname);
		serialize_long_long(metafile, addpool(poolfile, treefile, metaval[i], metatype[i]), metapos, fname);
	}

	long long geomstart = *geompos;

	serialize_byte(geomfile, mb_geometry[t], geompos, fname);
	serialize_long_long(geomfile, *seq, geompos, fname);

	serialize_long_long(geomfile, (layer << 2) | ((tippecanoe_minzoom != -1) << 1) | (tippecanoe_maxzoom != -1), geompos, fname);
	if (tippecanoe_minzoom != -1) {
		serialize_int(geomfile, tippecanoe_minzoom, geompos, fname);
	}
	if (tippecanoe_maxzoom != -1) {
		serialize_int(geomfile, tippecanoe_maxzoom, geompos, fname);
	}

	serialize_long_long(geomfile, metastart, geompos, fname);
	long long wx = initial_x, wy = initial_y;
	parse_geometry(t, coordinates, bbox, geompos, geomfile, VT_MOVETO, fname, jp, &wx, &wy, &initialized);
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
		for (feature_minzoom = 0; feature_minzoom < 31; feature_minzoom++) {
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

	if (*seq % 10000 == 0) {
		if (!quiet) {
			fprintf(stderr, "Read %.2f million features\r", *seq / 1000000.0);
		}
	}
	(*seq)++;

	return 1;
}

void parse_json(json_pull *jp, const char *reading, long long *seq, long long *metapos, long long *geompos, long long *indexpos, struct pool *exclude, struct pool *include, int exclude_all, FILE *metafile, FILE *geomfile, FILE *indexfile, struct memfile *poolfile, struct memfile *treefile, char *fname, int maxzoom, int basezoom, int layer, double droprate, long long *file_bbox) {
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

				serialize_geometry(j, NULL, reading, jp, seq, metapos, geompos, indexpos, exclude, include, exclude_all, metafile, geomfile, indexfile, poolfile, treefile, fname, maxzoom, basezoom, layer, droprate, file_bbox, NULL);
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
			int g;
			for (g = 0; g < geometries->length; g++) {
				serialize_geometry(geometries->array[g], properties, reading, jp, seq, metapos, geompos, indexpos, exclude, include, exclude_all, metafile, geomfile, indexfile, poolfile, treefile, fname, maxzoom, basezoom, layer, droprate, file_bbox, tippecanoe);
			}
		} else {
			serialize_geometry(geometry, properties, reading, jp, seq, metapos, geompos, indexpos, exclude, include, exclude_all, metafile, geomfile, indexfile, poolfile, treefile, fname, maxzoom, basezoom, layer, droprate, file_bbox, tippecanoe);
		}

		json_free(j);

		/* XXX check for any non-features in the outer object */
	}

	if (!quiet) {
		fprintf(stderr, "                              \r");
		//     (stderr, "Read 10000.00 million features\r", *seq / 1000000.0);
	}
}

int read_json(int argc, char **argv, char *fname, const char *layername, int maxzoom, int minzoom, int basezoom, sqlite3 *outdb, struct pool *exclude, struct pool *include, int exclude_all, double droprate, int buffer, const char *tmpdir, double gamma, char *prevent, char *additional) {
	int ret = EXIT_SUCCESS;

	char metaname[strlen(tmpdir) + strlen("/meta.XXXXXXXX") + 1];
	char poolname[strlen(tmpdir) + strlen("/pool.XXXXXXXX") + 1];
	char treename[strlen(tmpdir) + strlen("/tree.XXXXXXXX") + 1];
	char geomname[strlen(tmpdir) + strlen("/geom.XXXXXXXX") + 1];
	char indexname[strlen(tmpdir) + strlen("/index.XXXXXXXX") + 1];

	sprintf(metaname, "%s%s", tmpdir, "/meta.XXXXXXXX");
	sprintf(poolname, "%s%s", tmpdir, "/pool.XXXXXXXX");
	sprintf(treename, "%s%s", tmpdir, "/tree.XXXXXXXX");
	sprintf(geomname, "%s%s", tmpdir, "/geom.XXXXXXXX");
	sprintf(indexname, "%s%s", tmpdir, "/index.XXXXXXXX");

	int metafd = mkstemp(metaname);
	if (metafd < 0) {
		perror(metaname);
		exit(EXIT_FAILURE);
	}
	int poolfd = mkstemp(poolname);
	if (poolfd < 0) {
		perror(poolname);
		exit(EXIT_FAILURE);
	}
	int treefd = mkstemp(treename);
	if (treefd < 0) {
		perror(treename);
		exit(EXIT_FAILURE);
	}
	int geomfd = mkstemp(geomname);
	if (geomfd < 0) {
		perror(geomname);
		exit(EXIT_FAILURE);
	}
	int indexfd = mkstemp(indexname);
	if (indexfd < 0) {
		perror(indexname);
		exit(EXIT_FAILURE);
	}

	FILE *metafile = fopen(metaname, "wb");
	if (metafile == NULL) {
		perror(metaname);
		exit(EXIT_FAILURE);
	}
	struct memfile *poolfile = memfile_open(poolfd);
	if (poolfile == NULL) {
		perror(poolname);
		exit(EXIT_FAILURE);
	}
	struct memfile *treefile = memfile_open(treefd);
	if (treefile == NULL) {
		perror(treename);
		exit(EXIT_FAILURE);
	}
	FILE *geomfile = fopen(geomname, "wb");
	if (geomfile == NULL) {
		perror(geomname);
		exit(EXIT_FAILURE);
	}
	FILE *indexfile = fopen(indexname, "wb");
	if (indexfile == NULL) {
		perror(indexname);
		exit(EXIT_FAILURE);
	}
	long long metapos = 0;
	long long geompos = 0;
	long long indexpos = 0;

	unlink(metaname);
	unlink(poolname);
	unlink(treename);
	unlink(geomname);
	unlink(indexname);

	// To distinguish a null value
	{
		struct stringpool p;
		memfile_write(treefile, &p, sizeof(struct stringpool));
	}

	long long file_bbox[] = {UINT_MAX, UINT_MAX, 0, 0};
	unsigned midx = 0, midy = 0;
	long long seq = 0;

	int nlayers;
	if (layername != NULL) {
		nlayers = 1;
	} else {
		nlayers = argc;
		if (nlayers == 0) {
			nlayers = 1;
		}
	}

	int nsources = argc;
	if (nsources == 0) {
		nsources = 1;
	}

	int source;
	for (source = 0; source < nsources; source++) {
		json_pull *jp;
		const char *reading;
		FILE *fp;

		if (source >= argc) {
			reading = "standard input";
			fp = stdin;
		} else {
			reading = argv[source];
			fp = fopen(argv[source], "r");
			if (fp == NULL) {
				perror(argv[source]);
				continue;
			}
		}

		jp = json_begin_file(fp);

		int layer;
		if (nlayers == 1) {
			layer = 0;
		} else {
			layer = source;
		}

		parse_json(jp, reading, &seq, &metapos, &geompos, &indexpos, exclude, include, exclude_all, metafile, geomfile, indexfile, poolfile, treefile, fname, maxzoom, basezoom, layer, droprate, file_bbox);

		json_end(jp);
		fclose(fp);
	}

	fclose(metafile);
	fclose(geomfile);
	fclose(indexfile);
	memfile_close(treefile);

	struct stat geomst;
	struct stat metast;

	if (fstat(geomfd, &geomst) != 0) {
		perror("stat geom\n");
		exit(EXIT_FAILURE);
	}
	if (fstat(metafd, &metast) != 0) {
		perror("stat meta\n");
		exit(EXIT_FAILURE);
	}

	if (geomst.st_size == 0 || metast.st_size == 0) {
		fprintf(stderr, "did not read any valid geometries\n");
		exit(EXIT_FAILURE);
	}

	char *meta = (char *) mmap(NULL, metast.st_size, PROT_READ, MAP_PRIVATE, metafd, 0);
	if (meta == MAP_FAILED) {
		perror("mmap meta");
		exit(EXIT_FAILURE);
	}

	char *stringpool = poolfile->map;

	struct pool file_keys1[nlayers];
	struct pool *file_keys[nlayers];
	int i;
	for (i = 0; i < nlayers; i++) {
		pool_init(&file_keys1[i], 0);
		file_keys[i] = &file_keys1[i];
	}

	char *layernames[nlayers];
	for (i = 0; i < nlayers; i++) {
		if (layername != NULL) {
			layernames[i] = strdup(layername);
		} else {
			char *src = argv[i];
			if (argc < 1) {
				src = fname;
			}

			char *trunc = layernames[i] = malloc(strlen(src) + 1);
			const char *ocp, *use = src;
			for (ocp = src; *ocp; ocp++) {
				if (*ocp == '/' && ocp[1] != '\0') {
					use = ocp + 1;
				}
			}
			strcpy(trunc, use);

			char *cp = strstr(trunc, ".json");
			if (cp != NULL) {
				*cp = '\0';
			}
			cp = strstr(trunc, ".mbtiles");
			if (cp != NULL) {
				*cp = '\0';
			}

			char *out = trunc;
			for (cp = trunc; *cp; cp++) {
				if (isalpha(*cp) || isdigit(*cp) || *cp == '_') {
					*out++ = *cp;
				}
			}
			*out = '\0';

			if (!quiet) {
				printf("using layer %d name %s\n", i, trunc);
			}
		}
	}

	/* Sort the index by geometry */

	{
		int bytes = sizeof(struct index);
		if (!quiet) {
			fprintf(stderr, "Sorting %lld features\n", (long long) indexpos / bytes);
		}

		int page = sysconf(_SC_PAGESIZE);
		long long unit = (50 * 1024 * 1024 / bytes) * bytes;
		while (unit % page != 0) {
			unit += bytes;
		}

		int nmerges = (indexpos + unit - 1) / unit;
		struct merge merges[nmerges];

		long long start;
		for (start = 0; start < indexpos; start += unit) {
			long long end = start + unit;
			if (end > indexpos) {
				end = indexpos;
			}

			if (nmerges != 1) {
				if (!quiet) {
					fprintf(stderr, "Sorting part %lld of %d\r", start / unit + 1, nmerges);
				}
			}

			merges[start / unit].start = start;
			merges[start / unit].end = end;
			merges[start / unit].next = NULL;

			// MAP_PRIVATE to avoid disk writes if it fits in memory
			void *map = mmap(NULL, end - start, PROT_READ | PROT_WRITE, MAP_PRIVATE, indexfd, start);
			if (map == MAP_FAILED) {
				perror("mmap");
				exit(EXIT_FAILURE);
			}

			qsort(map, (end - start) / bytes, bytes, indexcmp);

			// Sorting and then copying avoids the need to
			// write out intermediate stages of the sort.

			void *map2 = mmap(NULL, end - start, PROT_READ | PROT_WRITE, MAP_SHARED, indexfd, start);
			if (map2 == MAP_FAILED) {
				perror("mmap (write)");
				exit(EXIT_FAILURE);
			}

			memcpy(map2, map, end - start);

			munmap(map, end - start);
			munmap(map2, end - start);
		}

		if (nmerges != 1) {
			if (!quiet) {
				fprintf(stderr, "\n");
			}
		}

		void *map = mmap(NULL, indexpos, PROT_READ, MAP_PRIVATE, indexfd, 0);
		if (map == MAP_FAILED) {
			perror("mmap");
			exit(EXIT_FAILURE);
		}

		FILE *f = fopen(indexname, "w");
		if (f == NULL) {
			perror(indexname);
			exit(EXIT_FAILURE);
		}

		merge(merges, nmerges, (unsigned char *) map, f, bytes, indexpos / bytes);

		munmap(map, indexpos);
		fclose(f);
		close(indexfd);
	}

	indexfd = open(indexname, O_RDONLY);
	if (indexfd < 0) {
		perror("reopen sorted index");
		exit(EXIT_FAILURE);
	}

	if (basezoom < 0 || droprate < 0) {
		struct index *map = mmap(NULL, indexpos, PROT_READ, MAP_PRIVATE, indexfd, 0);
		if (map == MAP_FAILED) {
			perror("mmap");
			exit(EXIT_FAILURE);
		}

#define MAX_ZOOM 30
		struct tile {
			unsigned x;
			unsigned y;
			long long count;
		} tile[MAX_ZOOM + 1], max[MAX_ZOOM + 1];

		{
			int i;
			for (i = 0; i <= MAX_ZOOM; i++) {
				tile[i].x = tile[i].y = tile[i].count = 0;
				max[i].x = max[i].y = max[i].count = 0;
			}
		}

		long long indices = indexpos / sizeof(struct index);
		long long i;
		for (i = 0; i < indices; i++) {
			unsigned xx, yy;
			decode(map[i].index, &xx, &yy);

			int z;
			for (z = 0; z <= MAX_ZOOM; z++) {
				unsigned xxx = 0, yyy = 0;
				if (z != 0) {
					xxx = xx >> (32 - z);
					yyy = yy >> (32 - z);
				}

				if (tile[z].x != xxx || tile[z].y != yyy) {
					if (tile[z].count > max[z].count) {
						max[z] = tile[z];
					}

					tile[z].x = xxx;
					tile[z].y = yyy;
					tile[z].count = 0;
				}

				tile[z].count++;
			}
		}

		int z;
		for (z = MAX_ZOOM; z >= 0; z--) {
			if (tile[z].count > max[z].count) {
				max[z] = tile[z];
			}
		}

#define MAX_FEATURES 50000

		if (basezoom < 0) {
			basezoom = MAX_ZOOM;

			for (z = MAX_ZOOM; z >= 0; z--) {
				if (max[z].count < MAX_FEATURES) {
					basezoom = z;
				}

				// printf("%d/%u/%u %lld\n", z, max[z].x, max[z].y, max[z].count);
			}

			fprintf(stderr, "Choosing a base zoom of -B%d to keep %lld features in tile %d/%u/%u.\n", basezoom, max[basezoom].count, basezoom, max[basezoom].x, max[basezoom].y);
		}

		if (droprate < 0) {
			droprate = 1;

			for (z = 0; z <= basezoom; z++) {
				double interval = exp(log(droprate) * (basezoom - z));

				if (max[z].count / interval >= MAX_FEATURES) {
					interval = (long double) max[z].count / MAX_FEATURES;
					droprate = exp(log(interval) / (basezoom - z));
					interval = exp(log(droprate) * (basezoom - z));

					fprintf(stderr, "Choosing a drop rate of -r%f to keep %f features in tile %d/%u/%u.\n", droprate, max[z].count / interval, z, max[z].x, max[z].y);
				}
			}
		}

		munmap(map, indexpos);
	}

	/* Copy geometries to a new file in index order */

	struct index *index_map = mmap(NULL, indexpos, PROT_READ, MAP_PRIVATE, indexfd, 0);
	if (index_map == MAP_FAILED) {
		perror("mmap index");
		exit(EXIT_FAILURE);
	}
	unlink(indexname);

	char *geom_map = mmap(NULL, geomst.st_size, PROT_READ, MAP_PRIVATE, geomfd, 0);
	if (geom_map == MAP_FAILED) {
		perror("mmap unsorted geometry");
		exit(EXIT_FAILURE);
	}
	if (close(geomfd) != 0) {
		perror("close unsorted geometry");
	}

	sprintf(geomname, "%s%s", tmpdir, "/geom.XXXXXXXX");
	geomfd = mkstemp(geomname);
	if (geomfd < 0) {
		perror(geomname);
		exit(EXIT_FAILURE);
	}
	geomfile = fopen(geomname, "wb");
	if (geomfile == NULL) {
		perror(geomname);
		exit(EXIT_FAILURE);
	}

	{
		geompos = 0;

		/* initial tile is 0/0/0 */
		serialize_int(geomfile, 0, &geompos, fname);
		serialize_uint(geomfile, 0, &geompos, fname);
		serialize_uint(geomfile, 0, &geompos, fname);

		long long i;
		long long sum = 0;
		long long progress = 0;
		for (i = 0; i < indexpos / sizeof(struct index); i++) {
			fwrite_check(geom_map + index_map[i].start, sizeof(char), index_map[i].end - index_map[i].start, geomfile, fname);
			sum += index_map[i].end - index_map[i].start;

			long long p = 1000 * i / (indexpos / sizeof(struct index));
			if (p != progress) {
				if (!quiet) {
					fprintf(stderr, "Reordering geometry: %3.1f%%\r", p / 10.0);
				}
				progress = p;
			}
		}

		/* end of tile */
		serialize_byte(geomfile, -2, &geompos, fname);
		fclose(geomfile);
	}

	if (munmap(index_map, indexpos) != 0) {
		perror("unmap sorted index");
	}
	if (munmap(geom_map, geomst.st_size) != 0) {
		perror("unmap unsorted geometry");
	}
	if (close(indexfd) != 0) {
		perror("close sorted index");
	}

	/* Traverse and split the geometries for each zoom level */

	geomfd = open(geomname, O_RDONLY);
	if (geomfd < 0) {
		perror("reopen sorted geometry");
		exit(EXIT_FAILURE);
	}
	unlink(geomname);
	if (fstat(geomfd, &geomst) != 0) {
		perror("stat sorted geom\n");
		exit(EXIT_FAILURE);
	}

	int fd[TEMP_FILES];
	off_t size[TEMP_FILES];

	fd[0] = geomfd;
	size[0] = geomst.st_size;

	int j;
	for (j = 1; j < TEMP_FILES; j++) {
		fd[j] = -1;
		size[j] = 0;
	}

	if (!quiet) {
		fprintf(stderr, "%lld features, %lld bytes of geometry, %lld bytes of metadata, %lld bytes of string pool\n", seq, (long long) geomst.st_size, (long long) metast.st_size, poolfile->off);
	}

	int written = traverse_zooms(fd, size, meta, stringpool, file_keys, &midx, &midy, layernames, maxzoom, minzoom, basezoom, outdb, droprate, buffer, fname, tmpdir, gamma, nlayers, prevent, additional, full_detail, low_detail, min_detail);

	if (maxzoom != written) {
		fprintf(stderr, "\n\n\n*** NOTE TILES ONLY COMPLETE THROUGH ZOOM %d ***\n\n\n", written);
		maxzoom = written;
		ret = EXIT_FAILURE;
	}

	if (munmap(meta, metast.st_size) != 0) {
		perror("munmap meta");
	}
	if (close(metafd) < 0) {
		perror("close meta");
	}

	if (memfile_close(poolfile) != 0) {
		perror("close pool");
	}

	double minlat = 0, minlon = 0, maxlat = 0, maxlon = 0, midlat = 0, midlon = 0;

	tile2latlon(midx, midy, maxzoom, &maxlat, &minlon);
	tile2latlon(midx + 1, midy + 1, maxzoom, &minlat, &maxlon);

	midlat = (maxlat + minlat) / 2;
	midlon = (maxlon + minlon) / 2;

	// If the bounding box extends off the plane on either side,
	// a feature wrapped across the date line, so the width of the
	// bounding box is the whole world.
	if (file_bbox[0] < 0) {
		file_bbox[0] = 0;
		file_bbox[2] = (1LL << 32) - 1;
	}
	if (file_bbox[2] > (1LL << 32) - 1) {
		file_bbox[0] = 0;
		file_bbox[2] = (1LL << 32) - 1;
	}
	if (file_bbox[1] < 0) {
		file_bbox[1] = 0;
	}
	if (file_bbox[3] > (1LL << 32) - 1) {
		file_bbox[3] = (1LL << 32) - 1;
	}

	tile2latlon(file_bbox[0], file_bbox[1], 32, &maxlat, &minlon);
	tile2latlon(file_bbox[2], file_bbox[3], 32, &minlat, &maxlon);

	if (midlat < minlat) {
		midlat = minlat;
	}
	if (midlat > maxlat) {
		midlat = maxlat;
	}
	if (midlon < minlon) {
		midlon = minlon;
	}
	if (midlon > maxlon) {
		midlon = maxlon;
	}

	mbtiles_write_metadata(outdb, fname, layernames, minzoom, maxzoom, minlat, minlon, maxlat, maxlon, midlat, midlon, file_keys, nlayers);

	for (i = 0; i < nlayers; i++) {
		pool_free_strings(&file_keys1[i]);
		free(layernames[i]);
	}
	return ret;
}

int main(int argc, char **argv) {
#ifdef MTRACE
	mtrace();
#endif

	init_cpus();

	extern int optind;
	extern char *optarg;
	int i;

	char *name = NULL;
	char *layer = NULL;
	char *outdir = NULL;
	int maxzoom = 14;
	int minzoom = 0;
	int basezoom = -1;
	int force = 0;
	double droprate = 2.5;
	double gamma = 0;
	int buffer = 5;
	const char *tmpdir = "/tmp";
	char prevent[256];
	char additional[256];

	struct pool exclude, include;
	pool_init(&exclude, 0);
	pool_init(&include, 0);
	int exclude_all = 0;

	for (i = 0; i < 256; i++) {
		prevent[i] = 0;
		additional[i] = 0;
	}

	while ((i = getopt(argc, argv, "l:n:z:Z:d:D:m:o:x:y:r:b:fXt:g:p:vqa:B:")) != -1) {
		switch (i) {
		case 'n':
			name = optarg;
			break;

		case 'l':
			layer = optarg;
			break;

		case 'z':
			maxzoom = atoi(optarg);
			break;

		case 'Z':
			minzoom = atoi(optarg);
			break;

		case 'B':
			if (strcmp(optarg, "g") == 0) {
				basezoom = -2;
			} else {
				basezoom = atoi(optarg);
			}
			break;

		case 'd':
			full_detail = atoi(optarg);
			break;

		case 'D':
			low_detail = atoi(optarg);
			break;

		case 'm':
			min_detail = atoi(optarg);
			break;

		case 'o':
			outdir = optarg;
			break;

		case 'x':
			pool(&exclude, optarg, VT_STRING);
			break;

		case 'y':
			exclude_all = 1;
			pool(&include, optarg, VT_STRING);
			break;

		case 'X':
			exclude_all = 1;
			break;

		case 'r':
			if (strcmp(optarg, "g") == 0) {
				droprate = -2;
			} else {
				droprate = atof(optarg);
			}
			break;

		case 'b':
			buffer = atoi(optarg);
			break;

		case 'f':
			force = 1;
			break;

		case 't':
			tmpdir = optarg;
			break;

		case 'g':
			gamma = atof(optarg);
			break;

		case 'q':
			quiet = 1;
			break;

		case 'p': {
			char *cp;
			for (cp = optarg; *cp != '\0'; cp++) {
				prevent[*cp & 0xFF] = 1;
			}
		} break;

		case 'a': {
			char *cp;
			for (cp = optarg; *cp != '\0'; cp++) {
				additional[*cp & 0xFF] = 1;
			}
		} break;

		case 'v':
			fprintf(stderr, VERSION);
			exit(EXIT_FAILURE);

		default:
			fprintf(stderr, "Usage: %s -o out.mbtiles [-n name] [-l layername] [-z maxzoom] [-Z minzoom] [-B basezoom] [-d detail] [-D lower-detail] [-m min-detail] [-x excluded-field ...] [-y included-field ...] [-X] [-r droprate] [-b buffer] [-t tmpdir] [-a rco] [-p sfkld] [-q] [file.json ...]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (minzoom > maxzoom) {
		fprintf(stderr, "minimum zoom -Z cannot be greater than maxzoom -z\n");
		exit(EXIT_FAILURE);
	}

	if (basezoom == -1) {
		basezoom = maxzoom;
	}

	if (full_detail <= 0) {
		full_detail = 12;
	}

	if (full_detail < min_detail || low_detail < min_detail) {
		fprintf(stderr, "%s: Full detail and low detail must be at least minimum detail\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	geometry_scale = 32 - (full_detail + maxzoom);

	if (outdir == NULL) {
		fprintf(stderr, "%s: must specify -o out.mbtiles\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (force) {
		unlink(outdir);
	}

	sqlite3 *outdb = mbtiles_open(outdir, argv);
	int ret = EXIT_SUCCESS;

	ret = read_json(argc - optind, argv + optind, name ? name : outdir, layer, maxzoom, minzoom, basezoom, outdb, &exclude, &include, exclude_all, droprate, buffer, tmpdir, gamma, prevent, additional);

	mbtiles_close(outdb, argv);

#ifdef MTRACE
	muntrace();
#endif

	return ret;
}
