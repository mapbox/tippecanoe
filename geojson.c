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
#include <getopt.h>

#ifdef __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/statfs.h>
#endif

#include "jsonpull.h"
#include "tile.h"
#include "pool.h"
#include "mbtiles.h"
#include "projection.h"
#include "version.h"
#include "memfile.h"

static int low_detail = 12;
static int full_detail = -1;
static int min_detail = 7;

int quiet = 0;
int geometry_scale = 0;

static int prevent[256];
static int additional[256];

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

struct source {
	char *layer;
	char *file;
	struct source *next;
};

int CPUS;
int TEMP_FILES;
long long MAX_FILES;
static long long diskfree;

#define MAX_ZOOM 24

struct reader {
	char *metaname;
	char *poolname;
	char *treename;
	char *geomname;
	char *indexname;

	int metafd;
	int poolfd;
	int treefd;
	int geomfd;
	int indexfd;

	FILE *metafile;
	struct memfile *poolfile;
	struct memfile *treefile;
	FILE *geomfile;
	FILE *indexfile;

	long long metapos;
	long long geompos;
	long long indexpos;

	long long *file_bbox;

	struct stat geomst;
	struct stat metast;

	char *geom_map;
};

void checkdisk(struct reader *r, int nreader) {
	long long used = 0;
	int i;
	for (i = 0; i < nreader; i++) {
		// Meta, pool, and tree are used once.
		// Geometry and index will be duplicated during sorting and tiling.
		used += r[i].metapos + 2 * r[i].geompos + 2 * r[i].indexpos + r[i].poolfile->len + r[i].treefile->len;
	}

	static int warned = 0;
	if (used > diskfree * .9 && !warned) {
		fprintf(stderr, "You will probably run out of disk space.\n%lld bytes used or committed, of %lld originally available\n", used, diskfree);
		warned = 1;
	}
};

void init_cpus() {
	CPUS = sysconf(_SC_NPROCESSORS_ONLN);
	if (CPUS < 1) {
		CPUS = 1;
	}
	/* XXX */ CPUS = 1;

	// Guard against short struct index.segment
	if (CPUS > 32767) {
		CPUS = 32767;
	}

	// Round down to a power of 2
	CPUS = 1 << (int) (log(CPUS) / log(2));

	struct rlimit rl;
	if (getrlimit(RLIMIT_NOFILE, &rl) != 0) {
		perror("getrlimit");
		exit(EXIT_FAILURE);
	} else {
		MAX_FILES = rl.rlim_cur;
	}

	// Don't really want too many temporary files, because the file system
	// will start to bog down eventually
	if (MAX_FILES > 2000) {
		MAX_FILES = 2000;
	}

	// MacOS can run out of system file descriptors
	// even if we stay under the rlimit, so try to
	// find out the real limit.
	long long fds[MAX_FILES];
	long long i;
	for (i = 0; i < MAX_FILES; i++) {
		fds[i] = open("/dev/null", O_RDONLY);
		if (fds[i] < 0) {
			break;
		}
	}
	long long j;
	for (j = 0; j < i; j++) {
		if (close(fds[j]) < 0) {
			perror("close");
			exit(EXIT_FAILURE);
		}
	}

	// Scale down because we really don't want to run the system out of files
	MAX_FILES = i * 3 / 4;
	if (MAX_FILES < 32) {
		fprintf(stderr, "Can't open a useful number of files: %lld\n", MAX_FILES);
		exit(EXIT_FAILURE);
	}

	TEMP_FILES = (MAX_FILES - 10) / 2;
	if (TEMP_FILES > CPUS * 4) {
		TEMP_FILES = CPUS * 4;
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

int deserialize_long_long_io(FILE *f, long long *n, long long *geompos) {
	unsigned long long zigzag = 0;
	int shift = 0;

	while (1) {
		int c = getc(f);
		if (c == EOF) {
			return 0;
		}
		(*geompos)++;

		if ((c & 0x80) == 0) {
			zigzag |= ((unsigned long long) c) << shift;
			shift += 7;
			break;
		} else {
			zigzag |= ((unsigned long long) (c & 0x7F)) << shift;
			shift += 7;
		}
	}

	*n = (zigzag >> 1) ^ (-(zigzag & 1));
	return 1;
}

int deserialize_int_io(FILE *f, int *n, long long *geompos) {
	long long ll = 0;
	int ret = deserialize_long_long_io(f, &ll, geompos);
	*n = ll;
	return ret;
}

int deserialize_uint_io(FILE *f, unsigned *n, long long *geompos) {
	if (fread(n, sizeof(unsigned), 1, f) != 1) {
		return 0;
	}
	*geompos += sizeof(unsigned);
	return 1;
}

int deserialize_byte_io(FILE *f, signed char *n, long long *geompos) {
	int c = getc(f);
	if (c == EOF) {
		return 0;
	}
	*n = c;
	(*geompos)++;
	return 1;
}

struct index {
	long long start;
	long long end;
	unsigned long long index;
	short segment;
	unsigned long long seq : (64 - 16);  // pack with segment to stay in 32 bytes
};

int indexcmp(const void *v1, const void *v2) {
	const struct index *i1 = (const struct index *) v1;
	const struct index *i2 = (const struct index *) v2;

	if (i1->index < i2->index) {
		return -1;
	} else if (i1->index > i2->index) {
		return 1;
	}

	if (i1->seq < i2->seq) {
		return -1;
	} else if (i1->seq > i2->seq) {
		return 1;
	}

	return 0;
}

struct merge {
	long long start;
	long long end;

	struct merge *next;
};

static void insert(struct merge *m, struct merge **head, unsigned char *map) {
	while (*head != NULL && indexcmp(map + m->start, map + (*head)->start) > 0) {
		head = &((*head)->next);
	}

	m->next = *head;
	*head = m;
}

static void merge(struct merge *merges, int nmerges, unsigned char *map, FILE *f, int bytes, long long nrec, char *geom_map, FILE *geom_out, long long *geompos, long long *progress, long long *progress_max, long long *progress_reported) {
	int i;
	struct merge *head = NULL;

	for (i = 0; i < nmerges; i++) {
		if (merges[i].start < merges[i].end) {
			insert(&(merges[i]), &head, map);
		}
	}

	while (head != NULL) {
		struct index *ix = (struct index *) (map + head->start);
		fwrite_check(geom_map + ix->start, 1, ix->end - ix->start, geom_out, "merge geometry");
		*geompos += ix->end - ix->start;

		// Count this as an 75%-accomplishment, since we already 25%-counted it
		*progress += (ix->end - ix->start) * 3 / 4;
		if (!quiet && 100 * *progress / *progress_max != *progress_reported) {
			fprintf(stderr, "Reordering geometry: %lld%% \r", 100 * *progress / *progress_max);
			*progress_reported = 100 * *progress / *progress_max;
		}

		fwrite_check(map + head->start, bytes, 1, f, "merge temporary");
		head->start += bytes;

		struct merge *m = head;
		head = m->next;
		m->next = NULL;

		if (m->start < m->end) {
			insert(m, &head, map);
		}
	}
}

struct stringpool {
	long long left;
	long long right;
	long long off;
};

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
	long long *sp = &treefile->tree;

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
	if (sp == &treefile->tree) {
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
		treefile->tree = p;
	} else {
		*((long long *) (treefile->map + ssp)) = p;
	}
	return off;
}

int serialize_geometry(json_object *geometry, json_object *properties, const char *reading, int line, volatile long long *layer_seq, volatile long long *progress_seq, long long *metapos, long long *geompos, long long *indexpos, struct pool *exclude, struct pool *include, int exclude_all, FILE *metafile, FILE *geomfile, FILE *indexfile, struct memfile *poolfile, struct memfile *treefile, const char *fname, int basezoom, int layer, double droprate, long long *file_bbox, json_object *tippecanoe, int segment, int *initialized, unsigned *initial_x, unsigned *initial_y, struct reader *readers) {
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
	char *metaval[nprop];
	int metatype[nprop];
	int mustfree[nprop];
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
			mustfree[m] = 0;

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
				metatype[m] = VT_STRING;
				metaval[m] = json_stringify(properties->values[i]);
				mustfree[m] = 1;
				m++;
			}
		}
	}

	for (i = 0; i < m; i++) {
		serialize_long_long(metafile, addpool(poolfile, treefile, metakey[i], VT_STRING), metapos, fname);
		serialize_long_long(metafile, addpool(poolfile, treefile, metaval[i], metatype[i]), metapos, fname);

		if (mustfree[i]) {
			free(metaval[i]);
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

void parse_json(json_pull *jp, const char *reading, volatile long long *layer_seq, volatile long long *progress_seq, long long *metapos, long long *geompos, long long *indexpos, struct pool *exclude, struct pool *include, int exclude_all, FILE *metafile, FILE *geomfile, FILE *indexfile, struct memfile *poolfile, struct memfile *treefile, char *fname, int basezoom, int layer, double droprate, long long *file_bbox, int segment, int *initialized, unsigned *initial_x, unsigned *initial_y, struct reader *readers) {
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

				serialize_geometry(j, NULL, reading, jp->line, layer_seq, progress_seq, metapos, geompos, indexpos, exclude, include, exclude_all, metafile, geomfile, indexfile, poolfile, treefile, fname, basezoom, layer, droprate, file_bbox, NULL, segment, initialized, initial_x, initial_y, readers);
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
				serialize_geometry(geometries->array[g], properties, reading, jp->line, layer_seq, progress_seq, metapos, geompos, indexpos, exclude, include, exclude_all, metafile, geomfile, indexfile, poolfile, treefile, fname, basezoom, layer, droprate, file_bbox, tippecanoe, segment, initialized, initial_x, initial_y, readers);
			}
		} else {
			serialize_geometry(geometry, properties, reading, jp->line, layer_seq, progress_seq, metapos, geompos, indexpos, exclude, include, exclude_all, metafile, geomfile, indexfile, poolfile, treefile, fname, basezoom, layer, droprate, file_bbox, tippecanoe, segment, initialized, initial_x, initial_y, readers);
		}

		json_free(j);

		/* XXX check for any non-features in the outer object */
	}
}

struct parse_json_args {
	json_pull *jp;
	const char *reading;
	volatile long long *layer_seq;
	volatile long long *progress_seq;
	long long *metapos;
	long long *geompos;
	long long *indexpos;
	struct pool *exclude;
	struct pool *include;
	int exclude_all;
	FILE *metafile;
	FILE *geomfile;
	FILE *indexfile;
	struct memfile *poolfile;
	struct memfile *treefile;
	char *fname;
	int basezoom;
	int layer;
	double droprate;
	long long *file_bbox;
	int segment;
	int *initialized;
	unsigned *initial_x;
	unsigned *initial_y;
	struct reader *readers;
};

void *run_parse_json(void *v) {
	struct parse_json_args *pja = v;

	parse_json(pja->jp, pja->reading, pja->layer_seq, pja->progress_seq, pja->metapos, pja->geompos, pja->indexpos, pja->exclude, pja->include, pja->exclude_all, pja->metafile, pja->geomfile, pja->indexfile, pja->poolfile, pja->treefile, pja->fname, pja->basezoom, pja->layer, pja->droprate, pja->file_bbox, pja->segment, pja->initialized, pja->initial_x, pja->initial_y, pja->readers);

	return NULL;
}

struct jsonmap {
	char *map;
	unsigned long long off;
	unsigned long long end;
};

ssize_t json_map_read(struct json_pull *jp, char *buffer, size_t n) {
	struct jsonmap *jm = jp->source;

	if (jm->off + n >= jm->end) {
		n = jm->end - jm->off;
	}

	memcpy(buffer, jm->map + jm->off, n);
	jm->off += n;

	return n;
}

struct json_pull *json_begin_map(char *map, long long len) {
	struct jsonmap *jm = malloc(sizeof(struct jsonmap));
	if (jm == NULL) {
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}

	jm->map = map;
	jm->off = 0;
	jm->end = len;

	return json_begin(json_map_read, jm);
}

struct sort_arg {
	int task;
	int cpus;
	long long indexpos;
	struct merge *merges;
	int indexfd;
	int nmerges;
	long long unit;
	int bytes;
};

void *run_sort(void *v) {
	struct sort_arg *a = v;

	long long start;
	for (start = a->task * a->unit; start < a->indexpos; start += a->unit * a->cpus) {
		long long end = start + a->unit;
		if (end > a->indexpos) {
			end = a->indexpos;
		}

		a->merges[start / a->unit].start = start;
		a->merges[start / a->unit].end = end;
		a->merges[start / a->unit].next = NULL;

		// MAP_PRIVATE to avoid disk writes if it fits in memory
		void *map = mmap(NULL, end - start, PROT_READ | PROT_WRITE, MAP_PRIVATE, a->indexfd, start);
		if (map == MAP_FAILED) {
			perror("mmap in run_sort");
			exit(EXIT_FAILURE);
		}
		madvise(map, end - start, MADV_RANDOM);
		madvise(map, end - start, MADV_WILLNEED);

		qsort(map, (end - start) / a->bytes, a->bytes, indexcmp);

		// Sorting and then copying avoids disk access to
		// write out intermediate stages of the sort.

		void *map2 = mmap(NULL, end - start, PROT_READ | PROT_WRITE, MAP_SHARED, a->indexfd, start);
		if (map2 == MAP_FAILED) {
			perror("mmap (write)");
			exit(EXIT_FAILURE);
		}
		madvise(map2, end - start, MADV_SEQUENTIAL);

		memcpy(map2, map, end - start);

		// No madvise, since caller will want the sorted data
		munmap(map, end - start);
		munmap(map2, end - start);
	}

	return NULL;
}

void do_read_parallel(char *map, long long len, long long initial_offset, const char *reading, struct reader *reader, volatile long long *progress_seq, struct pool *exclude, struct pool *include, int exclude_all, char *fname, int basezoom, int source, int nlayers, double droprate, int *initialized, unsigned *initial_x, unsigned *initial_y) {
	long long segs[CPUS + 1];
	segs[0] = 0;
	segs[CPUS] = len;

	int i;
	for (i = 1; i < CPUS; i++) {
		segs[i] = len * i / CPUS;

		while (segs[i] < len && map[segs[i]] != '\n') {
			segs[i]++;
		}
	}

	volatile long long layer_seq[CPUS];
	for (i = 0; i < CPUS; i++) {
		// To preserve feature ordering, unique id for each segment
		// begins with that segment's offset into the input
		layer_seq[i] = segs[i] + initial_offset;
	}

	struct parse_json_args pja[CPUS];
	pthread_t pthreads[CPUS];

	for (i = 0; i < CPUS; i++) {
		pja[i].jp = json_begin_map(map + segs[i], segs[i + 1] - segs[i]);
		pja[i].reading = reading;
		pja[i].layer_seq = &layer_seq[i];
		pja[i].progress_seq = progress_seq;
		pja[i].metapos = &reader[i].metapos;
		pja[i].geompos = &reader[i].geompos;
		pja[i].indexpos = &reader[i].indexpos;
		pja[i].exclude = exclude;
		pja[i].include = include;
		pja[i].exclude_all = exclude_all;
		pja[i].metafile = reader[i].metafile;
		pja[i].geomfile = reader[i].geomfile;
		pja[i].indexfile = reader[i].indexfile;
		pja[i].poolfile = reader[i].poolfile;
		pja[i].treefile = reader[i].treefile;
		pja[i].fname = fname;
		pja[i].basezoom = basezoom;
		pja[i].layer = source < nlayers ? source : 0;
		pja[i].droprate = droprate;
		pja[i].file_bbox = reader[i].file_bbox;
		pja[i].segment = i;
		pja[i].initialized = &initialized[i];
		pja[i].initial_x = &initial_x[i];
		pja[i].initial_y = &initial_y[i];
		pja[i].readers = reader;

		if (pthread_create(&pthreads[i], NULL, run_parse_json, &pja[i]) != 0) {
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
	}

	for (i = 0; i < CPUS; i++) {
		void *retval;

		if (pthread_join(pthreads[i], &retval) != 0) {
			perror("pthread_join");
		}

		free(pja[i].jp->source);
		json_end(pja[i].jp);
	}
}

struct read_parallel_arg {
	int fd;
	FILE *fp;
	long long offset;
	long long len;
	volatile int *is_parsing;

	const char *reading;
	struct reader *reader;
	volatile long long *progress_seq;
	struct pool *exclude;
	struct pool *include;
	int exclude_all;
	char *fname;
	int maxzoom;
	int basezoom;
	int source;
	int nlayers;
	double droprate;
	int *initialized;
	unsigned *initial_x;
	unsigned *initial_y;
};

void *run_read_parallel(void *v) {
	struct read_parallel_arg *a = v;

	struct stat st;
	if (fstat(a->fd, &st) != 0) {
		perror("stat read temp");
	}
	if (a->len != st.st_size) {
		fprintf(stderr, "wrong number of bytes in temporary: %lld vs %lld\n", a->len, (long long) st.st_size);
	}
	a->len = st.st_size;

	char *map = mmap(NULL, a->len, PROT_READ, MAP_PRIVATE, a->fd, 0);
	if (map == NULL || map == MAP_FAILED) {
		perror("map intermediate input");
		exit(EXIT_FAILURE);
	}
	madvise(map, a->len, MADV_RANDOM);  // sequential, but from several pointers at once

	do_read_parallel(map, a->len, a->offset, a->reading, a->reader, a->progress_seq, a->exclude, a->include, a->exclude_all, a->fname, a->basezoom, a->source, a->nlayers, a->droprate, a->initialized, a->initial_x, a->initial_y);

	madvise(map, a->len, MADV_DONTNEED);
	if (munmap(map, a->len) != 0) {
		perror("munmap source file");
	}
	if (fclose(a->fp) != 0) {
		perror("close source file");
		exit(EXIT_FAILURE);
	}

	*(a->is_parsing) = 0;
	free(a);

	return NULL;
}

void start_parsing(int fd, FILE *fp, long long offset, long long len, volatile int *is_parsing, pthread_t *parallel_parser, const char *reading, struct reader *reader, volatile long long *progress_seq, struct pool *exclude, struct pool *include, int exclude_all, char *fname, int basezoom, int source, int nlayers, double droprate, int *initialized, unsigned *initial_x, unsigned *initial_y) {
	// This has to kick off an intermediate thread to start the parser threads,
	// so the main thread can get back to reading the next input stage while
	// the intermediate thread waits for the completion of the parser threads.

	*is_parsing = 1;

	struct read_parallel_arg *rpa = malloc(sizeof(struct read_parallel_arg));
	if (rpa == NULL) {
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}

	rpa->fd = fd;
	rpa->fp = fp;
	rpa->offset = offset;
	rpa->len = len;
	rpa->is_parsing = is_parsing;

	rpa->reading = reading;
	rpa->reader = reader;
	rpa->progress_seq = progress_seq;
	rpa->exclude = exclude;
	rpa->include = include;
	rpa->exclude_all = exclude_all;
	rpa->fname = fname;
	rpa->basezoom = basezoom;
	rpa->source = source;
	rpa->nlayers = nlayers;
	rpa->droprate = droprate;
	rpa->initialized = initialized;
	rpa->initial_x = initial_x;
	rpa->initial_y = initial_y;

	if (pthread_create(parallel_parser, NULL, run_read_parallel, rpa) != 0) {
		perror("pthread_create");
		exit(EXIT_FAILURE);
	}
}

void radix1(int *geomfds_in, int *indexfds_in, int inputs, int prefix, int splits, long long mem, const char *tmpdir, long long *availfiles, FILE *geomfile, FILE *indexfile, long long *geompos_out, long long *progress, long long *progress_max, long long *progress_reported) {
	// Arranged as bits to facilitate subdividing again if a subdivided file is still huge
	int splitbits = log(splits) / log(2);
	splits = 1 << splitbits;

	FILE *geomfiles[splits];
	FILE *indexfiles[splits];
	int geomfds[splits];
	int indexfds[splits];
	long long sub_geompos[splits];

	int i;
	for (i = 0; i < splits; i++) {
		sub_geompos[i] = 0;

		char geomname[strlen(tmpdir) + strlen("/geom.XXXXXXXX") + 1];
		sprintf(geomname, "%s%s", tmpdir, "/geom.XXXXXXXX");
		char indexname[strlen(tmpdir) + strlen("/index.XXXXXXXX") + 1];
		sprintf(indexname, "%s%s", tmpdir, "/index.XXXXXXXX");

		geomfds[i] = mkstemp(geomname);
		if (geomfds[i] < 0) {
			perror(geomname);
			exit(EXIT_FAILURE);
		}
		indexfds[i] = mkstemp(indexname);
		if (indexfds[i] < 0) {
			perror(indexname);
			exit(EXIT_FAILURE);
		}

		geomfiles[i] = fopen(geomname, "wb");
		if (geomfiles[i] == NULL) {
			perror(geomname);
			exit(EXIT_FAILURE);
		}
		indexfiles[i] = fopen(indexname, "wb");
		if (indexfiles[i] == NULL) {
			perror(indexname);
			exit(EXIT_FAILURE);
		}

		*availfiles -= 4;

		unlink(geomname);
		unlink(indexname);
	}

	for (i = 0; i < inputs; i++) {
		struct stat geomst, indexst;
		if (fstat(geomfds_in[i], &geomst) < 0) {
			perror("stat geom");
			exit(EXIT_FAILURE);
		}
		if (fstat(indexfds_in[i], &indexst) < 0) {
			perror("stat index");
			exit(EXIT_FAILURE);
		}

		if (indexst.st_size != 0) {
			struct index *indexmap = mmap(NULL, indexst.st_size, PROT_READ, MAP_PRIVATE, indexfds_in[i], 0);
			if (indexmap == MAP_FAILED) {
				fprintf(stderr, "fd %lld, len %lld\n", (long long) indexfds_in[i], (long long) indexst.st_size);
				perror("map index");
				exit(EXIT_FAILURE);
			}
			madvise(indexmap, indexst.st_size, MADV_SEQUENTIAL);
			madvise(indexmap, indexst.st_size, MADV_WILLNEED);
			char *geommap = mmap(NULL, geomst.st_size, PROT_READ, MAP_PRIVATE, geomfds_in[i], 0);
			if (geommap == MAP_FAILED) {
				perror("map geom");
				exit(EXIT_FAILURE);
			}
			madvise(geommap, geomst.st_size, MADV_SEQUENTIAL);
			madvise(geommap, geomst.st_size, MADV_WILLNEED);

			long long a;
			for (a = 0; a < indexst.st_size / sizeof(struct index); a++) {
				struct index ix = indexmap[a];
				unsigned long long which = (ix.index << prefix) >> (64 - splitbits);
				long long pos = sub_geompos[which];

				fwrite_check(geommap + ix.start, ix.end - ix.start, 1, geomfiles[which], "geom");
				sub_geompos[which] += ix.end - ix.start;

				// Count this as a 25%-accomplishment, since we will copy again
				*progress += (ix.end - ix.start) / 4;
				if (!quiet && 100 * *progress / *progress_max != *progress_reported) {
					fprintf(stderr, "Reordering geometry: %lld%% \r", 100 * *progress / *progress_max);
					*progress_reported = 100 * *progress / *progress_max;
				}

				ix.start = pos;
				ix.end = sub_geompos[which];

				fwrite_check(&ix, sizeof(struct index), 1, indexfiles[which], "index");
			}

			madvise(indexmap, indexst.st_size, MADV_DONTNEED);
			if (munmap(indexmap, indexst.st_size) < 0) {
				perror("unmap index");
				exit(EXIT_FAILURE);
			}
			madvise(geommap, geomst.st_size, MADV_DONTNEED);
			if (munmap(geommap, geomst.st_size) < 0) {
				perror("unmap geom");
				exit(EXIT_FAILURE);
			}
		}

		if (close(geomfds_in[i]) < 0) {
			perror("close geom");
			exit(EXIT_FAILURE);
		}
		if (close(indexfds_in[i]) < 0) {
			perror("close index");
			exit(EXIT_FAILURE);
		}

		*availfiles += 2;
	}

	for (i = 0; i < splits; i++) {
		if (fclose(geomfiles[i]) != 0) {
			perror("fclose geom");
			exit(EXIT_FAILURE);
		}
		if (fclose(indexfiles[i]) != 0) {
			perror("fclose index");
			exit(EXIT_FAILURE);
		}

		*availfiles += 2;
	}

	for (i = 0; i < splits; i++) {
		int already_closed = 0;

		struct stat geomst, indexst;
		if (fstat(geomfds[i], &geomst) < 0) {
			perror("stat geom");
			exit(EXIT_FAILURE);
		}
		if (fstat(indexfds[i], &indexst) < 0) {
			perror("stat index");
			exit(EXIT_FAILURE);
		}

		if (indexst.st_size > 0) {
			if (indexst.st_size > sizeof(struct index) && indexst.st_size + geomst.st_size < mem) {
				long long indexpos = indexst.st_size;
				int bytes = sizeof(struct index);

				int page = sysconf(_SC_PAGESIZE);
				// Don't try to sort more than 2GB at once,
				// which used to crash Macs and may still
				long long max_unit = 2LL * 1024 * 1024 * 1024;
				long long unit = ((indexpos / CPUS + bytes - 1) / bytes) * bytes;
				if (unit > max_unit) {
					unit = max_unit;
				}
				unit = ((unit + page - 1) / page) * page;

				int nmerges = (indexpos + unit - 1) / unit;
				struct merge merges[nmerges];

				int a;
				for (a = 0; a < nmerges; a++) {
					merges[a].start = merges[a].end = 0;
				}

				pthread_t pthreads[CPUS];
				struct sort_arg args[CPUS];

				for (a = 0; a < CPUS; a++) {
					args[a].task = a;
					args[a].cpus = CPUS;
					args[a].indexpos = indexpos;
					args[a].merges = merges;
					args[a].indexfd = indexfds[i];
					args[a].nmerges = nmerges;
					args[a].unit = unit;
					args[a].bytes = bytes;

					if (pthread_create(&pthreads[a], NULL, run_sort, &args[a]) != 0) {
						perror("pthread_create");
						exit(EXIT_FAILURE);
					}
				}

				for (a = 0; a < CPUS; a++) {
					void *retval;

					if (pthread_join(pthreads[a], &retval) != 0) {
						perror("pthread_join");
					}
				}

				struct indexmap *indexmap = mmap(NULL, indexst.st_size, PROT_READ, MAP_PRIVATE, indexfds[i], 0);
				if (indexmap == MAP_FAILED) {
					fprintf(stderr, "fd %lld, len %lld\n", (long long) indexfds[i], (long long) indexst.st_size);
					perror("map index");
					exit(EXIT_FAILURE);
				}
				madvise(indexmap, indexst.st_size, MADV_RANDOM);  // sequential, but from several pointers at once
				madvise(indexmap, indexst.st_size, MADV_WILLNEED);
				char *geommap = mmap(NULL, geomst.st_size, PROT_READ, MAP_PRIVATE, geomfds[i], 0);
				if (geommap == MAP_FAILED) {
					perror("map geom");
					exit(EXIT_FAILURE);
				}
				madvise(geommap, geomst.st_size, MADV_RANDOM);
				madvise(geommap, geomst.st_size, MADV_WILLNEED);

				merge(merges, nmerges, (unsigned char *) indexmap, indexfile, bytes, indexpos / bytes, geommap, geomfile, geompos_out, progress, progress_max, progress_reported);

				madvise(indexmap, indexst.st_size, MADV_DONTNEED);
				if (munmap(indexmap, indexst.st_size) < 0) {
					perror("unmap index");
					exit(EXIT_FAILURE);
				}
				madvise(geommap, geomst.st_size, MADV_DONTNEED);
				if (munmap(geommap, geomst.st_size) < 0) {
					perror("unmap geom");
					exit(EXIT_FAILURE);
				}
			} else if (indexst.st_size == sizeof(struct index) || prefix + splitbits >= 64) {
				struct index *indexmap = mmap(NULL, indexst.st_size, PROT_READ, MAP_PRIVATE, indexfds[i], 0);
				if (indexmap == MAP_FAILED) {
					fprintf(stderr, "fd %lld, len %lld\n", (long long) indexfds[i], (long long) indexst.st_size);
					perror("map index");
					exit(EXIT_FAILURE);
				}
				madvise(indexmap, indexst.st_size, MADV_SEQUENTIAL);
				madvise(indexmap, indexst.st_size, MADV_WILLNEED);
				char *geommap = mmap(NULL, geomst.st_size, PROT_READ, MAP_PRIVATE, geomfds[i], 0);
				if (geommap == MAP_FAILED) {
					perror("map geom");
					exit(EXIT_FAILURE);
				}
				madvise(geommap, geomst.st_size, MADV_RANDOM);
				madvise(geommap, geomst.st_size, MADV_WILLNEED);

				long long a;
				for (a = 0; a < indexst.st_size / sizeof(struct index); a++) {
					struct index ix = indexmap[a];
					long long pos = *geompos_out;

					fwrite_check(geommap + ix.start, ix.end - ix.start, 1, geomfile, "geom");
					*geompos_out += ix.end - ix.start;

					// Count this as an 75%-accomplishment, since we already 25%-counted it
					*progress += (ix.end - ix.start) * 3 / 4;
					if (!quiet && 100 * *progress / *progress_max != *progress_reported) {
						fprintf(stderr, "Reordering geometry: %lld%% \r", 100 * *progress / *progress_max);
						*progress_reported = 100 * *progress / *progress_max;
					}

					ix.start = pos;
					ix.end = *geompos_out;
					fwrite_check(&ix, sizeof(struct index), 1, indexfile, "index");
				}

				madvise(indexmap, indexst.st_size, MADV_DONTNEED);
				if (munmap(indexmap, indexst.st_size) < 0) {
					perror("unmap index");
					exit(EXIT_FAILURE);
				}
				madvise(geommap, geomst.st_size, MADV_DONTNEED);
				if (munmap(geommap, geomst.st_size) < 0) {
					perror("unmap geom");
					exit(EXIT_FAILURE);
				}
			} else {
				// We already reported the progress from splitting this radix out
				// but we need to split it again, which will be credited with more
				// progress. So increase the total amount of progress to report by
				// the additional progress that will happpen, which may move the
				// counter backward but will be an honest estimate of the work remaining.
				*progress_max += geomst.st_size / 4;

				radix1(&geomfds[i], &indexfds[i], 1, prefix + splitbits, *availfiles / 4, mem, tmpdir, availfiles, geomfile, indexfile, geompos_out, progress, progress_max, progress_reported);
				already_closed = 1;
			}
		}

		if (!already_closed) {
			if (close(geomfds[i]) < 0) {
				perror("close geom");
				exit(EXIT_FAILURE);
			}
			if (close(indexfds[i]) < 0) {
				perror("close index");
				exit(EXIT_FAILURE);
			}

			*availfiles += 2;
		}
	}
}

void radix(struct reader *reader, int nreaders, FILE *geomfile, int geomfd, FILE *indexfile, int indexfd, const char *tmpdir, long long *geompos) {
	// Run through the index and geometry for each reader,
	// splitting the contents out by index into as many
	// sub-files as we can write to simultaneously.

	// Then sort each of those by index, recursively if it is
	// too big to fit in memory.

	// Then concatenate each of the sub-outputs into a final output.

	long long mem;

#ifdef __APPLE__
	int64_t hw_memsize;
	size_t len = sizeof(int64_t);
	if (sysctlbyname("hw.memsize", &hw_memsize, &len, NULL, 0) < 0) {
		perror("sysctl hw.memsize");
		exit(EXIT_FAILURE);
	}
	mem = hw_memsize;
#else
	long long pagesize = sysconf(_SC_PAGESIZE);
	long long pages = sysconf(_SC_PHYS_PAGES);
	if (pages < 0 || pagesize < 0) {
		perror("sysconf _SC_PAGESIZE or _SC_PHYS_PAGES");
		exit(EXIT_FAILURE);
	}

	mem = (long long) pages * pagesize;
#endif

	// Just for code coverage testing. Deeply recursive sorting is very slow
	// compared to sorting in memory.
	if (additional[A_PREFER_RADIX_SORT]) {
		mem = 8192;
	}

	long long availfiles = MAX_FILES - 2 * nreaders  // each reader has a geom and an index
			       - 4			 // pool, meta, mbtiles, mbtiles journal
			       - 4			 // top-level geom and index output, both FILE and fd
			       - 3;			 // stdin, stdout, stderr

	// 4 because for each we have output and input FILE and fd for geom and index
	int splits = availfiles / 4;

	// Be somewhat conservative about memory availability because the whole point of this
	// is to keep from thrashing by working on chunks that will fit in memory.
	mem /= 2;

	long long geom_total = 0;
	int geomfds[nreaders];
	int indexfds[nreaders];
	int i;
	for (i = 0; i < nreaders; i++) {
		geomfds[i] = reader[i].geomfd;
		indexfds[i] = reader[i].indexfd;

		struct stat geomst;
		if (fstat(reader[i].geomfd, &geomst) < 0) {
			perror("stat geom");
			exit(EXIT_FAILURE);
		}
		geom_total += geomst.st_size;
	}

	long long progress = 0, progress_max = geom_total, progress_reported = -1;
	long long availfiles_before = availfiles;
	radix1(geomfds, indexfds, nreaders, 0, splits, mem, tmpdir, &availfiles, geomfile, indexfile, geompos, &progress, &progress_max, &progress_reported);

	if (availfiles - 2 * nreaders != availfiles_before) {
		fprintf(stderr, "Internal error: miscounted available file descriptors: %lld vs %lld\n", availfiles - 2 * nreaders, availfiles);
		exit(EXIT_FAILURE);
	}
}

int read_json(int argc, struct source **sourcelist, char *fname, const char *layername, int maxzoom, int minzoom, int basezoom, double basezoom_marker_width, sqlite3 *outdb, struct pool *exclude, struct pool *include, int exclude_all, double droprate, int buffer, const char *tmpdir, double gamma, int *prevent, int *additional, int read_parallel, int forcetable, const char *attribution) {
	int ret = EXIT_SUCCESS;

	struct reader reader[CPUS];
	int i;
	for (i = 0; i < CPUS; i++) {
		struct reader *r = reader + i;

		r->metaname = malloc(strlen(tmpdir) + strlen("/meta.XXXXXXXX") + 1);
		r->poolname = malloc(strlen(tmpdir) + strlen("/pool.XXXXXXXX") + 1);
		r->treename = malloc(strlen(tmpdir) + strlen("/tree.XXXXXXXX") + 1);
		r->geomname = malloc(strlen(tmpdir) + strlen("/geom.XXXXXXXX") + 1);
		r->indexname = malloc(strlen(tmpdir) + strlen("/index.XXXXXXXX") + 1);

		if (r->metaname == NULL || r->poolname == NULL || r->treename == NULL || r->geomname == NULL || r->indexname == NULL) {
			perror("Out of memory");
			exit(EXIT_FAILURE);
		}

		sprintf(r->metaname, "%s%s", tmpdir, "/meta.XXXXXXXX");
		sprintf(r->poolname, "%s%s", tmpdir, "/pool.XXXXXXXX");
		sprintf(r->treename, "%s%s", tmpdir, "/tree.XXXXXXXX");
		sprintf(r->geomname, "%s%s", tmpdir, "/geom.XXXXXXXX");
		sprintf(r->indexname, "%s%s", tmpdir, "/index.XXXXXXXX");

		r->metafd = mkstemp(r->metaname);
		if (r->metafd < 0) {
			perror(r->metaname);
			exit(EXIT_FAILURE);
		}
		r->poolfd = mkstemp(r->poolname);
		if (r->poolfd < 0) {
			perror(r->poolname);
			exit(EXIT_FAILURE);
		}
		r->treefd = mkstemp(r->treename);
		if (r->treefd < 0) {
			perror(r->treename);
			exit(EXIT_FAILURE);
		}
		r->geomfd = mkstemp(r->geomname);
		if (r->geomfd < 0) {
			perror(r->geomname);
			exit(EXIT_FAILURE);
		}
		r->indexfd = mkstemp(r->indexname);
		if (r->indexfd < 0) {
			perror(r->indexname);
			exit(EXIT_FAILURE);
		}

		r->metafile = fopen(r->metaname, "wb");
		if (r->metafile == NULL) {
			perror(r->metaname);
			exit(EXIT_FAILURE);
		}
		r->poolfile = memfile_open(r->poolfd);
		if (r->poolfile == NULL) {
			perror(r->poolname);
			exit(EXIT_FAILURE);
		}
		r->treefile = memfile_open(r->treefd);
		if (r->treefile == NULL) {
			perror(r->treename);
			exit(EXIT_FAILURE);
		}
		r->geomfile = fopen(r->geomname, "wb");
		if (r->geomfile == NULL) {
			perror(r->geomname);
			exit(EXIT_FAILURE);
		}
		r->indexfile = fopen(r->indexname, "wb");
		if (r->indexfile == NULL) {
			perror(r->indexname);
			exit(EXIT_FAILURE);
		}
		r->metapos = 0;
		r->geompos = 0;
		r->indexpos = 0;

		unlink(r->metaname);
		unlink(r->poolname);
		unlink(r->treename);
		unlink(r->geomname);
		unlink(r->indexname);

		// To distinguish a null value
		{
			struct stringpool p;
			memfile_write(r->treefile, &p, sizeof(struct stringpool));
		}
		// Keep metadata file from being completely empty if no attributes
		serialize_int(r->metafile, 0, &r->metapos, "meta");

		r->file_bbox = malloc(4 * sizeof(long long));
		if (r->file_bbox == NULL) {
			perror("Out of memory");
			exit(EXIT_FAILURE);
		}
		r->file_bbox[0] = r->file_bbox[1] = UINT_MAX;
		r->file_bbox[2] = r->file_bbox[3] = 0;
	}

	struct statfs fsstat;
	if (fstatfs(reader[0].geomfd, &fsstat) != 0) {
		perror("fstatfs");
		exit(EXIT_FAILURE);
	}
	diskfree = (long long) fsstat.f_bsize * fsstat.f_bavail;

	volatile long long progress_seq = 0;

	int initialized[CPUS];
	unsigned initial_x[CPUS], initial_y[CPUS];
	for (i = 0; i < CPUS; i++) {
		initialized[i] = initial_x[i] = initial_y[i] = 0;
	}

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

	long overall_offset = 0;

	int source;
	for (source = 0; source < nsources; source++) {
		const char *reading;
		int fd;

		if (source >= argc) {
			reading = "standard input";
			fd = 0;
		} else {
			reading = sourcelist[source]->file;
			fd = open(sourcelist[source]->file, O_RDONLY);
			if (fd < 0) {
				perror(sourcelist[source]->file);
				continue;
			}
		}

		struct stat st;
		char *map = NULL;
		off_t off = 0;

		if (read_parallel) {
			if (fstat(fd, &st) == 0) {
				off = lseek(fd, 0, SEEK_CUR);
				if (off >= 0) {
					map = mmap(NULL, st.st_size - off, PROT_READ, MAP_PRIVATE, fd, off);
					// No error if MAP_FAILED because check is below
					if (map != MAP_FAILED) {
						madvise(map, st.st_size - off, MADV_RANDOM);  // sequential, but from several pointers at once
					}
				}
			}
		}

		if (map != NULL && map != MAP_FAILED) {
			do_read_parallel(map, st.st_size - off, overall_offset, reading, reader, &progress_seq, exclude, include, exclude_all, fname, basezoom, source, nlayers, droprate, initialized, initial_x, initial_y);
			overall_offset += st.st_size - off;
			checkdisk(reader, CPUS);

			if (munmap(map, st.st_size - off) != 0) {
				madvise(map, st.st_size, MADV_DONTNEED);
				perror("munmap source file");
			}
		} else {
			FILE *fp = fdopen(fd, "r");
			if (fp == NULL) {
				perror(sourcelist[source]->file);
				if (close(fd) != 0) {
					perror("close source file");
					exit(EXIT_FAILURE);
				}
				continue;
			}

			if (read_parallel) {
				// Serial reading of chunks that are then parsed in parallel

				char readname[strlen(tmpdir) + strlen("/read.XXXXXXXX") + 1];
				sprintf(readname, "%s%s", tmpdir, "/read.XXXXXXXX");
				int readfd = mkstemp(readname);
				if (readfd < 0) {
					perror(readname);
					exit(EXIT_FAILURE);
				}
				FILE *readfp = fdopen(readfd, "w");
				if (readfp == NULL) {
					perror(readname);
					exit(EXIT_FAILURE);
				}
				unlink(readname);

				volatile int is_parsing = 0;
				long long ahead = 0;
				long long initial_offset = overall_offset;
				pthread_t parallel_parser;

#define READ_BUF 2000
#define PARSE_MIN 10000000
#define PARSE_MAX (1LL * 1024 * 1024 * 1024)

				char buf[READ_BUF];
				int n;

				while ((n = fread(buf, sizeof(char), READ_BUF, fp)) > 0) {
					fwrite_check(buf, sizeof(char), n, readfp, reading);
					ahead += n;

					if (buf[n - 1] == '\n' && ahead > PARSE_MIN) {
						// Don't let the streaming reader get too far ahead of the parsers.
						// If the buffered input gets huge, even if the parsers are still running,
						// wait for the parser thread instead of continuing to stream input.

						if (is_parsing == 0 || ahead >= PARSE_MAX) {
							if (initial_offset != 0) {
								if (pthread_join(parallel_parser, NULL) != 0) {
									perror("pthread_join");
									exit(EXIT_FAILURE);
								}
							}

							fflush(readfp);
							start_parsing(readfd, readfp, initial_offset, ahead, &is_parsing, &parallel_parser, reading, reader, &progress_seq, exclude, include, exclude_all, fname, basezoom, source, nlayers, droprate, initialized, initial_x, initial_y);

							initial_offset += ahead;
							overall_offset += ahead;
							checkdisk(reader, CPUS);
							ahead = 0;

							sprintf(readname, "%s%s", tmpdir, "/read.XXXXXXXX");
							readfd = mkstemp(readname);
							if (readfd < 0) {
								perror(readname);
								exit(EXIT_FAILURE);
							}
							readfp = fdopen(readfd, "w");
							if (readfp == NULL) {
								perror(readname);
								exit(EXIT_FAILURE);
							}
							unlink(readname);
						}
					}
				}
				if (n < 0) {
					perror(reading);
				}

				if (initial_offset != 0) {
					if (pthread_join(parallel_parser, NULL) != 0) {
						perror("pthread_join");
						exit(EXIT_FAILURE);
					}
				}

				fflush(readfp);

				if (ahead > 0) {
					start_parsing(readfd, readfp, initial_offset, ahead, &is_parsing, &parallel_parser, reading, reader, &progress_seq, exclude, include, exclude_all, fname, basezoom, source, nlayers, droprate, initialized, initial_x, initial_y);

					if (pthread_join(parallel_parser, NULL) != 0) {
						perror("pthread_join");
					}

					overall_offset += ahead;
					checkdisk(reader, CPUS);
				}
			} else {
				// Plain serial reading

				long long layer_seq = overall_offset;
				json_pull *jp = json_begin_file(fp);
				parse_json(jp, reading, &layer_seq, &progress_seq, &reader[0].metapos, &reader[0].geompos, &reader[0].indexpos, exclude, include, exclude_all, reader[0].metafile, reader[0].geomfile, reader[0].indexfile, reader[0].poolfile, reader[0].treefile, fname, basezoom, source < nlayers ? source : 0, droprate, reader[0].file_bbox, 0, &initialized[0], &initial_x[0], &initial_y[0], reader);
				json_end(jp);
				overall_offset = layer_seq;
				checkdisk(reader, CPUS);
			}

			if (fclose(fp) != 0) {
				perror("fclose input");
				exit(EXIT_FAILURE);
			}
		}
	}

	if (!quiet) {
		fprintf(stderr, "                              \r");
		//     (stderr, "Read 10000.00 million features\r", *progress_seq / 1000000.0);
	}

	for (i = 0; i < CPUS; i++) {
		if (fclose(reader[i].metafile) != 0) {
			perror("fclose meta");
			exit(EXIT_FAILURE);
		}
		if (fclose(reader[i].geomfile) != 0) {
			perror("fclose geom");
			exit(EXIT_FAILURE);
		}
		if (fclose(reader[i].indexfile) != 0) {
			perror("fclose index");
			exit(EXIT_FAILURE);
		}
		memfile_close(reader[i].treefile);

		if (fstat(reader[i].geomfd, &reader[i].geomst) != 0) {
			perror("stat geom\n");
			exit(EXIT_FAILURE);
		}
		if (fstat(reader[i].metafd, &reader[i].metast) != 0) {
			perror("stat meta\n");
			exit(EXIT_FAILURE);
		}
	}

	struct pool file_keys1[nlayers];
	struct pool *file_keys[nlayers];
	for (i = 0; i < nlayers; i++) {
		pool_init(&file_keys1[i], 0);
		file_keys[i] = &file_keys1[i];
	}

	char *layernames[nlayers];
	for (i = 0; i < nlayers; i++) {
		if (layername != NULL) {
			layernames[i] = strdup(layername);
			if (layernames[i] == NULL) {
				perror("Out of memory");
				exit(EXIT_FAILURE);
			}
		} else {
			char *src;
			if (argc < 1) {
				src = fname;
			} else if (sourcelist[i]->layer != NULL) {
				src = sourcelist[i]->layer;
			} else {
				src = sourcelist[i]->file;
			}

			char *trunc = layernames[i] = malloc(strlen(src) + 1);
			if (trunc == NULL) {
				perror("Out of memory");
				exit(EXIT_FAILURE);
			}

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
				fprintf(stderr, "For layer %d, using name \"%s\"\n", i, trunc);
			}
		}
	}

	// Create a combined string pool and a combined metadata file
	// but keep track of the offsets into it since we still need
	// segment+offset to find the data.

	long long pool_off[CPUS];
	long long meta_off[CPUS];

	char poolname[strlen(tmpdir) + strlen("/pool.XXXXXXXX") + 1];
	sprintf(poolname, "%s%s", tmpdir, "/pool.XXXXXXXX");

	int poolfd = mkstemp(poolname);
	if (poolfd < 0) {
		perror(poolname);
		exit(EXIT_FAILURE);
	}

	FILE *poolfile = fopen(poolname, "wb");
	if (poolfile == NULL) {
		perror(poolname);
		exit(EXIT_FAILURE);
	}

	unlink(poolname);

	char metaname[strlen(tmpdir) + strlen("/meta.XXXXXXXX") + 1];
	sprintf(metaname, "%s%s", tmpdir, "/meta.XXXXXXXX");

	int metafd = mkstemp(metaname);
	if (metafd < 0) {
		perror(metaname);
		exit(EXIT_FAILURE);
	}

	FILE *metafile = fopen(metaname, "wb");
	if (metafile == NULL) {
		perror(metaname);
		exit(EXIT_FAILURE);
	}

	unlink(metaname);

	long long metapos = 0;
	long long poolpos = 0;

	for (i = 0; i < CPUS; i++) {
		if (reader[i].metapos > 0) {
			void *map = mmap(NULL, reader[i].metapos, PROT_READ, MAP_PRIVATE, reader[i].metafd, 0);
			if (map == MAP_FAILED) {
				perror("mmap unmerged meta");
				exit(EXIT_FAILURE);
			}
			madvise(map, reader[i].metapos, MADV_SEQUENTIAL);
			madvise(map, reader[i].metapos, MADV_WILLNEED);
			if (fwrite(map, reader[i].metapos, 1, metafile) != 1) {
				perror("Reunify meta");
				exit(EXIT_FAILURE);
			}
			madvise(map, reader[i].metapos, MADV_DONTNEED);
			if (munmap(map, reader[i].metapos) != 0) {
				perror("unmap unmerged meta");
			}
		}

		meta_off[i] = metapos;
		metapos += reader[i].metapos;
		if (close(reader[i].metafd) != 0) {
			perror("close unmerged meta");
		}

		if (reader[i].poolfile->off > 0) {
			if (fwrite(reader[i].poolfile->map, reader[i].poolfile->off, 1, poolfile) != 1) {
				perror("Reunify string pool");
				exit(EXIT_FAILURE);
			}
		}

		pool_off[i] = poolpos;
		poolpos += reader[i].poolfile->off;
		memfile_close(reader[i].poolfile);
	}

	if (fclose(poolfile) != 0) {
		perror("fclose pool");
		exit(EXIT_FAILURE);
	}
	if (fclose(metafile) != 0) {
		perror("fclose meta");
		exit(EXIT_FAILURE);
	}

	char *meta = (char *) mmap(NULL, metapos, PROT_READ, MAP_PRIVATE, metafd, 0);
	if (meta == MAP_FAILED) {
		perror("mmap meta");
		exit(EXIT_FAILURE);
	}
	madvise(meta, metapos, MADV_RANDOM);

	char *stringpool = NULL;
	if (poolpos > 0) {  // Will be 0 if -X was specified
		stringpool = (char *) mmap(NULL, poolpos, PROT_READ, MAP_PRIVATE, poolfd, 0);
		if (stringpool == MAP_FAILED) {
			perror("mmap string pool");
			exit(EXIT_FAILURE);
		}
		madvise(stringpool, poolpos, MADV_RANDOM);
	}

	char indexname[strlen(tmpdir) + strlen("/index.XXXXXXXX") + 1];
	sprintf(indexname, "%s%s", tmpdir, "/index.XXXXXXXX");

	int indexfd = mkstemp(indexname);
	if (indexfd < 0) {
		perror(indexname);
		exit(EXIT_FAILURE);
	}
	FILE *indexfile = fopen(indexname, "wb");
	if (indexfile == NULL) {
		perror(indexname);
		exit(EXIT_FAILURE);
	}

	unlink(indexname);

	char geomname[strlen(tmpdir) + strlen("/geom.XXXXXXXX") + 1];
	sprintf(geomname, "%s%s", tmpdir, "/geom.XXXXXXXX");

	int geomfd = mkstemp(geomname);
	if (geomfd < 0) {
		perror(geomname);
		exit(EXIT_FAILURE);
	}
	FILE *geomfile = fopen(geomname, "wb");
	if (geomfile == NULL) {
		perror(geomname);
		exit(EXIT_FAILURE);
	}
	unlink(geomname);

	long long geompos = 0;

	/* initial tile is 0/0/0 */
	serialize_int(geomfile, 0, &geompos, fname);
	serialize_uint(geomfile, 0, &geompos, fname);
	serialize_uint(geomfile, 0, &geompos, fname);

	radix(reader, CPUS, geomfile, geomfd, indexfile, indexfd, tmpdir, &geompos);

	/* end of tile */
	serialize_byte(geomfile, -2, &geompos, fname);

	if (fclose(geomfile) != 0) {
		perror("fclose geom");
		exit(EXIT_FAILURE);
	}
	if (fclose(indexfile) != 0) {
		perror("fclose index");
		exit(EXIT_FAILURE);
	}

	struct stat indexst;
	if (fstat(indexfd, &indexst) < 0) {
		perror("stat index");
		exit(EXIT_FAILURE);
	}
	long long indexpos = indexst.st_size;
	progress_seq = indexpos / sizeof(struct index);

	if (!quiet) {
		fprintf(stderr, "%lld features, %lld bytes of geometry, %lld bytes of metadata, %lld bytes of string pool\n", progress_seq, geompos, metapos, poolpos);
	}

	if (indexpos == 0) {
		fprintf(stderr, "Did not read any valid geometries\n");
		exit(EXIT_FAILURE);
	}

	if (basezoom < 0 || droprate < 0) {
		struct index *map = mmap(NULL, indexpos, PROT_READ, MAP_PRIVATE, indexfd, 0);
		if (map == MAP_FAILED) {
			perror("mmap index for basezoom");
			exit(EXIT_FAILURE);
		}
		madvise(map, indexpos, MADV_SEQUENTIAL);
		madvise(map, indexpos, MADV_WILLNEED);

		struct tile {
			unsigned x;
			unsigned y;
			long long count;
			long long fullcount;
			double gap;
			unsigned long long previndex;
		} tile[MAX_ZOOM + 1], max[MAX_ZOOM + 1];

		{
			int i;
			for (i = 0; i <= MAX_ZOOM; i++) {
				tile[i].x = tile[i].y = tile[i].count = tile[i].fullcount = tile[i].gap = tile[i].previndex = 0;
				max[i].x = max[i].y = max[i].count = max[i].fullcount = 0;
			}
		}

		long long progress = -1;

		long long indices = indexpos / sizeof(struct index);
		long long i;
		for (i = 0; i < indices; i++) {
			unsigned xx, yy;
			decode(map[i].index, &xx, &yy);

			long long nprogress = 100 * i / indices;
			if (nprogress != progress) {
				progress = nprogress;
				if (!quiet) {
					fprintf(stderr, "Base zoom/drop rate: %lld%% \r", progress);
				}
			}

			int z;
			for (z = 0; z <= MAX_ZOOM; z++) {
				unsigned xxx = 0, yyy = 0;
				if (z != 0) {
					xxx = xx >> (32 - z);
					yyy = yy >> (32 - z);
				}

				double scale = (double) (1LL << (64 - 2 * (z + 8)));

				if (tile[z].x != xxx || tile[z].y != yyy) {
					if (tile[z].count > max[z].count) {
						max[z] = tile[z];
					}

					tile[z].x = xxx;
					tile[z].y = yyy;
					tile[z].count = 0;
					tile[z].fullcount = 0;
					tile[z].gap = 0;
					tile[z].previndex = 0;
				}

				tile[z].fullcount++;

				if (manage_gap(map[i].index, &tile[z].previndex, scale, gamma, &tile[z].gap)) {
					continue;
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

		int max_features = 50000 / (basezoom_marker_width * basezoom_marker_width);

		int obasezoom = basezoom;
		if (basezoom < 0) {
			basezoom = MAX_ZOOM;

			for (z = MAX_ZOOM; z >= 0; z--) {
				if (max[z].count < max_features) {
					basezoom = z;
				}

				// printf("%d/%u/%u %lld\n", z, max[z].x, max[z].y, max[z].count);
			}

			fprintf(stderr, "Choosing a base zoom of -B%d to keep %lld features in tile %d/%u/%u.\n", basezoom, max[basezoom].count, basezoom, max[basezoom].x, max[basezoom].y);
		}

		if (obasezoom < 0 && basezoom > maxzoom) {
			fprintf(stderr, "Couldn't find a suitable base zoom. Working from the other direction.\n");
			if (gamma == 0) {
				fprintf(stderr, "You might want to try -g1 to limit near-duplicates.\n");
			}

			if (droprate < 0) {
				if (maxzoom == 0) {
					droprate = 2.5;
				} else {
					droprate = exp(log((long double) max[0].count / max[maxzoom].count) / (maxzoom));
					fprintf(stderr, "Choosing a drop rate of -r%f to get from %lld to %lld in %d zooms\n", droprate, max[maxzoom].count, max[0].count, maxzoom);
				}
			}

			basezoom = 0;
			for (z = 0; z <= maxzoom; z++) {
				double zoomdiff = log((long double) max[z].count / max_features) / log(droprate);
				if (zoomdiff + z > basezoom) {
					basezoom = ceil(zoomdiff + z);
				}
			}

			fprintf(stderr, "Choosing a base zoom of -B%d to keep %f features in tile %d/%u/%u.\n", basezoom, max[maxzoom].count * exp(log(droprate) * (maxzoom - basezoom)), maxzoom, max[maxzoom].x, max[maxzoom].y);
		} else if (droprate < 0) {
			droprate = 1;

			for (z = basezoom - 1; z >= 0; z--) {
				double interval = exp(log(droprate) * (basezoom - z));

				if (max[z].count / interval >= max_features) {
					interval = (long double) max[z].count / max_features;
					droprate = exp(log(interval) / (basezoom - z));
					interval = exp(log(droprate) * (basezoom - z));

					fprintf(stderr, "Choosing a drop rate of -r%f to keep %f features in tile %d/%u/%u.\n", droprate, max[z].count / interval, z, max[z].x, max[z].y);
				}
			}
		}

		if (gamma > 0) {
			int effective = 0;

			for (z = 0; z < maxzoom; z++) {
				if (max[z].count < max[z].fullcount) {
					effective = z + 1;
				}
			}

			if (effective == 0) {
				fprintf(stderr, "With gamma, effective base zoom is 0, so no effective drop rate\n");
			} else {
				double interval_0 = exp(log(droprate) * (basezoom - 0));
				double interval_eff = exp(log(droprate) * (basezoom - effective));
				if (effective > basezoom) {
					interval_eff = 1;
				}

				double scaled_0 = max[0].count / interval_0;
				double scaled_eff = max[effective].count / interval_eff;

				double rate_at_0 = scaled_0 / max[0].fullcount;
				double rate_at_eff = scaled_eff / max[effective].fullcount;

				double eff_drop = exp(log(rate_at_eff / rate_at_0) / (effective - 0));

				fprintf(stderr, "With gamma, effective base zoom of %d, effective drop rate of %f\n", effective, eff_drop);
			}
		}

		madvise(map, indexpos, MADV_DONTNEED);
		munmap(map, indexpos);
	}

	if (close(indexfd) != 0) {
		perror("close sorted index");
	}

	/* Traverse and split the geometries for each zoom level */

	struct stat geomst;
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

	unsigned midx = 0, midy = 0;
	int written = traverse_zooms(fd, size, meta, stringpool, file_keys, &midx, &midy, layernames, maxzoom, minzoom, basezoom, outdb, droprate, buffer, fname, tmpdir, gamma, nlayers, prevent, additional, full_detail, low_detail, min_detail, meta_off, pool_off, initial_x, initial_y);

	if (maxzoom != written) {
		fprintf(stderr, "\n\n\n*** NOTE TILES ONLY COMPLETE THROUGH ZOOM %d ***\n\n\n", written);
		maxzoom = written;
		ret = EXIT_FAILURE;
	}

	madvise(meta, metapos, MADV_DONTNEED);
	if (munmap(meta, metapos) != 0) {
		perror("munmap meta");
	}
	if (close(metafd) < 0) {
		perror("close meta");
	}

	if (poolpos > 0) {
		madvise(pool, poolpos, MADV_DONTNEED);
		if (munmap(stringpool, poolpos) != 0) {
			perror("munmap stringpool");
		}
	}
	if (close(poolfd) < 0) {
		perror("close pool");
	}

	double minlat = 0, minlon = 0, maxlat = 0, maxlon = 0, midlat = 0, midlon = 0;

	tile2latlon(midx, midy, maxzoom, &maxlat, &minlon);
	tile2latlon(midx + 1, midy + 1, maxzoom, &minlat, &maxlon);

	midlat = (maxlat + minlat) / 2;
	midlon = (maxlon + minlon) / 2;

	long long file_bbox[4] = {UINT_MAX, UINT_MAX, 0, 0};
	for (i = 0; i < CPUS; i++) {
		if (reader[i].file_bbox[0] < file_bbox[0]) {
			file_bbox[0] = reader[i].file_bbox[0];
		}
		if (reader[i].file_bbox[1] < file_bbox[1]) {
			file_bbox[1] = reader[i].file_bbox[1];
		}
		if (reader[i].file_bbox[2] > file_bbox[2]) {
			file_bbox[2] = reader[i].file_bbox[2];
		}
		if (reader[i].file_bbox[3] > file_bbox[3]) {
			file_bbox[3] = reader[i].file_bbox[3];
		}
	}

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

	mbtiles_write_metadata(outdb, fname, layernames, minzoom, maxzoom, minlat, minlon, maxlat, maxlon, midlat, midlon, file_keys, nlayers, forcetable, attribution);

	for (i = 0; i < nlayers; i++) {
		pool_free_strings(&file_keys1[i]);
		free(layernames[i]);
	}

	return ret;
}

int int_in(int v, int *a, int len) {
	int i;

	for (i = 0; i < len; i++) {
		if (a[i] == v) {
			return 1;
		}
	}

	return 0;
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
	double basezoom_marker_width = 1;
	int force = 0;
	int forcetable = 0;
	double droprate = 2.5;
	double gamma = 0;
	int buffer = 5;
	const char *tmpdir = "/tmp";
	const char *attribution = NULL;

	int nsources = 0;
	struct source *sources = NULL;

	struct pool exclude, include;
	pool_init(&exclude, 0);
	pool_init(&include, 0);
	int exclude_all = 0;
	int read_parallel = 0;
	int files_open_at_start;

	for (i = 0; i < 256; i++) {
		prevent[i] = 0;
		additional[i] = 0;
	}

	{
		char dup[256];

		memset(dup, 0, sizeof(dup));
		for (i = 0; i < sizeof(additional_options) / sizeof(additional_options[0]); i++) {
			if (dup[additional_options[i]]) {
				fprintf(stderr, "Internal error: reused -a%c\n", additional_options[i]);
				exit(EXIT_FAILURE);
			}
			dup[additional_options[i]] = 1;
		}

		memset(dup, 0, sizeof(dup));
		for (i = 0; i < sizeof(prevent_options) / sizeof(prevent_options[0]); i++) {
			if (dup[prevent_options[i]]) {
				fprintf(stderr, "Internal error: reused -p%c\n", prevent_options[i]);
				exit(EXIT_FAILURE);
			}
			dup[prevent_options[i]] = 1;
		}
	}

	static struct option long_options[] = {
		{"name", required_argument, 0, 'n'},
		{"layer", required_argument, 0, 'l'},
		{"attribution", required_argument, 0, 'A'},
		{"named-layer", required_argument, 0, 'L'},
		{"maximum-zoom", required_argument, 0, 'z'},
		{"minimum-zoom", required_argument, 0, 'Z'},
		{"base-zoom", required_argument, 0, 'B'},
		{"full-detail", required_argument, 0, 'd'},
		{"low-detail", required_argument, 0, 'D'},
		{"minimum-detail", required_argument, 0, 'm'},
		{"output", required_argument, 0, 'o'},
		{"exclude", required_argument, 0, 'x'},
		{"include", required_argument, 0, 'y'},
		{"drop-rate", required_argument, 0, 'r'},
		{"buffer", required_argument, 0, 'b'},
		{"temporary-directory", required_argument, 0, 't'},
		{"gamma", required_argument, 0, 'g'},
		{"prevent", required_argument, 0, 'p'},
		{"additional", required_argument, 0, 'a'},

		{"exclude-all", no_argument, 0, 'X'},
		{"force", no_argument, 0, 'f'},
		{"allow-existing", no_argument, 0, 'F'},
		{"quiet", no_argument, 0, 'q'},
		{"version", no_argument, 0, 'v'},
		{"read-parallel", no_argument, 0, 'P'},

		{"coalesce", no_argument, &additional[A_COALESCE], 1},
		{"reverse", no_argument, &additional[A_REVERSE], 1},
		{"reorder", no_argument, &additional[A_REORDER], 1},
		{"drop-lines", no_argument, &additional[A_LINE_DROP], 1},
		{"drop-polygons", no_argument, &additional[A_POLYGON_DROP], 1},
		{"prefer-radix-sort", no_argument, &additional[A_PREFER_RADIX_SORT], 1},

		{"no-line-simplification", no_argument, &prevent[P_SIMPLIFY], 1},
		{"simplify-only-low-zooms", no_argument, &prevent[P_SIMPLIFY_LOW], 1},
		{"no-feature-limit", no_argument, &prevent[P_FEATURE_LIMIT], 1},
		{"no-tile-size-limit", no_argument, &prevent[P_KILOBYTE_LIMIT], 1},
		{"force-feature-limit", no_argument, &prevent[P_DYNAMIC_DROP], 1},
		{"preseve-input-order", no_argument, &prevent[P_INPUT_ORDER], 1},
		{"no-polygon-splitting", no_argument, &prevent[P_POLYGON_SPLIT], 1},

		{0, 0, 0, 0},
	};

	while ((i = getopt_long(argc, argv, "n:l:z:Z:B:d:D:m:o:x:y:r:b:t:g:p:a:XfFqvPL:A:", long_options, NULL)) != -1) {
		switch (i) {
		case 0:
			break;

		case 'n':
			name = optarg;
			break;

		case 'l':
			layer = optarg;
			break;

		case 'A':
			attribution = optarg;
			break;

		case 'L': {
			char *cp = strchr(optarg, ':');
			if (cp == NULL || cp == optarg) {
				fprintf(stderr, "%s: -L requires layername:file\n", argv[0]);
				exit(EXIT_FAILURE);
			}
			struct source *src = malloc(sizeof(struct source));
			if (src == NULL) {
				perror("Out of memory");
				exit(EXIT_FAILURE);
			}

			src->layer = strdup(optarg);
			src->file = strdup(cp + 1);
			if (src->layer == NULL || src->file == NULL) {
				perror("Out of memory");
				exit(EXIT_FAILURE);
			}
			src->layer[cp - optarg] = '\0';
			src->next = sources;
			sources = src;
			nsources++;
		} break;

		case 'z':
			maxzoom = atoi(optarg);
			break;

		case 'Z':
			minzoom = atoi(optarg);
			break;

		case 'B':
			if (strcmp(optarg, "g") == 0) {
				basezoom = -2;
			} else if (optarg[0] == 'g' || optarg[0] == 'f') {
				basezoom = -2;
				if (optarg[0] == 'g') {
					basezoom_marker_width = atof(optarg + 1);
				} else {
					basezoom_marker_width = sqrt(50000 / atof(optarg + 1));
				}
				if (basezoom_marker_width == 0 || atof(optarg + 1) == 0) {
					fprintf(stderr, "%s: Must specify value >0 with -B%c\n", argv[0], optarg[0]);
					exit(EXIT_FAILURE);
				}
			} else {
				basezoom = atoi(optarg);
				if (basezoom == 0 && strcmp(optarg, "0") != 0) {
					fprintf(stderr, "%s: Couldn't understand -B%s\n", argv[0], optarg);
					exit(EXIT_FAILURE);
				}
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
			} else if (optarg[0] == 'g' || optarg[0] == 'f') {
				droprate = -2;
				if (optarg[0] == 'g') {
					basezoom_marker_width = atof(optarg + 1);
				} else {
					basezoom_marker_width = sqrt(50000 / atof(optarg + 1));
				}
				if (basezoom_marker_width == 0 || atof(optarg + 1) == 0) {
					fprintf(stderr, "%s: Must specify value >0 with -r%c\n", argv[0], optarg[0]);
					exit(EXIT_FAILURE);
				}
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

		case 'F':
			forcetable = 1;
			break;

		case 't':
			tmpdir = optarg;
			if (tmpdir[0] != '/') {
				fprintf(stderr, "Warning: temp directory %s doesn't begin with /\n", tmpdir);
			}
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
				if (int_in(*cp, prevent_options, sizeof(prevent_options) / sizeof(prevent_options[0]))) {
					prevent[*cp & 0xFF] = 1;
				} else {
					fprintf(stderr, "%s: Unknown option -p%c\n", argv[0], *cp);
					exit(EXIT_FAILURE);
				}
			}
		} break;

		case 'a': {
			char *cp;
			for (cp = optarg; *cp != '\0'; cp++) {
				if (int_in(*cp, additional_options, sizeof(additional_options) / sizeof(additional_options[0]))) {
					additional[*cp & 0xFF] = 1;
				} else {
					fprintf(stderr, "%s: Unknown option -a%c\n", argv[0], *cp);
					exit(EXIT_FAILURE);
				}
			}
		} break;

		case 'v':
			fprintf(stderr, VERSION);
			exit(EXIT_FAILURE);

		case 'P':
			read_parallel = 1;
			break;

		default:
			fprintf(stderr, "Usage: %s -o out.mbtiles [-n name] [-l source] [-z maxzoom] [-Z minzoom] [-B basezoom] [-d detail] [-D lower-detail] [-m min-detail] [-x excluded-field ...] [-y included-field ...] [-X] [-r droprate] [-b buffer] [-t tmpdir] [-a rco] [-p sfkld] [-q] [-P] [file.json ...]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	files_open_at_start = open("/dev/null", O_RDONLY);
	close(files_open_at_start);

	if (maxzoom > MAX_ZOOM) {
		maxzoom = MAX_ZOOM;
		fprintf(stderr, "Highest supported zoom is %d\n", maxzoom);
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
	if (geometry_scale < 0) {
		geometry_scale = 0;
		fprintf(stderr, "Full detail + maxzoom > 32, so you are asking for more detail than is available.\n");
	}

	if ((basezoom < 0 || droprate < 0) && (gamma < 0)) {
		// Can't use randomized (as opposed to evenly distributed) dot dropping
		// if rate and base aren't known during feature reading.
		gamma = 0;
		fprintf(stderr, "Forcing -g0 since -B or -r is not known\n");
	}

	if (outdir == NULL) {
		fprintf(stderr, "%s: must specify -o out.mbtiles\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (force) {
		unlink(outdir);
	}

	sqlite3 *outdb = mbtiles_open(outdir, argv, forcetable);
	int ret = EXIT_SUCCESS;

	for (i = optind; i < argc; i++) {
		struct source *src = malloc(sizeof(struct source));
		if (src == NULL) {
			perror("Out of memory");
			exit(EXIT_FAILURE);
		}

		src->layer = NULL;
		src->file = argv[i];
		src->next = sources;
		sources = src;
		nsources++;
	}

	struct source *sourcelist[nsources];
	i = nsources - 1;
	for (; sources != NULL; sources = sources->next) {
		sourcelist[i--] = sources;
	}

	ret = read_json(nsources, sourcelist, name ? name : outdir, layer, maxzoom, minzoom, basezoom, basezoom_marker_width, outdb, &exclude, &include, exclude_all, droprate, buffer, tmpdir, gamma, prevent, additional, read_parallel, forcetable, attribution);

	mbtiles_close(outdb, argv);

#ifdef MTRACE
	muntrace();
#endif

	i = open("/dev/null", O_RDONLY);
	// i < files_open_at_start is not an error, because reading from a pipe closes stdin
	if (i > files_open_at_start) {
		fprintf(stderr, "Internal error: did not close all files: %d\n", i);
		exit(EXIT_FAILURE);
	}

	return ret;
}
