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
#include "jsonpull.h"

#define GEOM_POINT 0                /* array of positions */
#define GEOM_MULTIPOINT 1           /* array of arrays of positions */
#define GEOM_LINESTRING 2           /* array of arrays of positions */
#define GEOM_MULTILINESTRING 3      /* array of arrays of arrays of positions */
#define GEOM_POLYGON 4              /* array of arrays of arrays of positions */
#define GEOM_MULTIPOLYGON 5         /* array of arrays of arrays of arrays of positions */
#define GEOM_TYPES 6

#define VT_END 0
#define VT_POINT 1
#define VT_LINE 2
#define VT_POLYGON 3

#define VT_MOVETO 1
#define VT_LINETO 2
#define VT_CLOSEPATH 7

#define VT_STRING 1
#define VT_NUMBER 2
#define VT_BOOLEAN 7

char *geometry_names[GEOM_TYPES] = {
	"Point",
	"MultiPoint",
	"LineString",
	"MultiLineString",
	"Polygon",
	"MultiPolygon",
};

int geometry_within[GEOM_TYPES] = {
	-1,                /* point */
	GEOM_POINT,        /* multipoint */
	GEOM_POINT,        /* linestring */
	GEOM_LINESTRING,   /* multilinestring */
	GEOM_LINESTRING,   /* polygon */
	GEOM_POLYGON,      /* multipolygon */
};

int mb_geometry[GEOM_TYPES] = {
	VT_POINT,
	VT_POINT,
	VT_LINE,
	VT_LINE,
	VT_POLYGON,
	VT_POLYGON,
};

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
void latlon2tile(double lat, double lon, int zoom, unsigned int *x, unsigned int *y) {
	double lat_rad = lat * M_PI / 180;
	unsigned long long n = 1LL << zoom;

	*x = n * ((lon + 180) / 360);
	*y = n * (1 - (log(tan(lat_rad) + 1/cos(lat_rad)) / M_PI)) / 2;
}

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
void tile2latlon(unsigned int x, unsigned int y, int zoom, double *lat, double *lon) {
	unsigned long long n = 1LL << zoom;
	*lon = 360.0 * x / n - 180.0;
	float lat_rad = atan(sinh(M_PI * (1 - 2.0 * y / n)));
	*lat = lat_rad * 180 / M_PI;
}

unsigned long long encode(unsigned int wx, unsigned int wy) {
	long long out = 0;

	int i;
	for (i = 0; i < 32; i++) {
		long long v = ((wx >> (32 - (i + 1))) & 1) << 1;
		v |= (wy >> (32 - (i + 1))) & 1;
		v = v << (64 - 2 * (i + 1));

		out |= v;
	}


	return out;
}

void decode(unsigned long long index, unsigned *wx, unsigned *wy) {
	*wx = *wy = 0;

	int i;
	for (i = 0; i < 32; i++) {
		*wx |= ((index >> (64 - 2 * (i + 1) + 1)) & 1) << (32 - (i + 1));
		*wy |= ((index >> (64 - 2 * (i + 1) + 0)) & 1) << (32 - (i + 1));
	}
}

// http://www.tbray.org/ongoing/When/200x/2003/03/22/Binary
void *search(const void *key, const void *base, size_t nel, size_t width,
		int (*cmp)(const void *, const void *)) {

	long long high = nel, low = -1, probe;
	while (high - low > 1) {
		probe = (low + high) >> 1;
		int c = cmp(((char *) base) + probe * width, key);
		if (c > 0) {
			high = probe;
		} else {
			low = probe;
		}
	}

	if (low < 0) {
		low = 0;
	}

	return ((char *) base) + low * width;
}

struct index {
	unsigned long long index;
	long long fpos;

	struct index *next;
};

int indexcmp(const void *v1, const void *v2) {
	const struct index *i1 = v1;
	const struct index *i2 = v2;

	if (i1->index < i2->index) {
		return -1;
	} else if (i1->index > i2->index) {
		return 1;
	} else {
		return 0;
	}
}

size_t fwrite_check(const void *ptr, size_t size, size_t nitems, FILE *stream) {
	size_t w = fwrite(ptr, size, nitems, stream);
	if (w != nitems) {
		fprintf(stderr, "Write failed\n");
		exit(EXIT_FAILURE);
	}
	return w;
}

void serialize_int(FILE *out, int n, long long *fpos) {
	fwrite_check(&n, sizeof(int), 1, out);
	*fpos += sizeof(int);
}

void serialize_uint(FILE *out, unsigned n, long long *fpos) {
	fwrite_check(&n, sizeof(unsigned), 1, out);
	*fpos += sizeof(unsigned);
}

void serialize_string(FILE *out, char *s, long long *fpos) {
	int len = strlen(s);

	serialize_int(out, len, fpos);
	fwrite_check(s, sizeof(char), len, out);
	*fpos += len;
}

void parse_geometry(int t, json_object *j, unsigned *bbox, long long *fpos, FILE *out, int op) {
	if (j == NULL || j->type != JSON_ARRAY) {
		fprintf(stderr, "expected array for type %d\n", t);
		return;
	}

	int within = geometry_within[t];
	if (within >= 0) {
		int i;
		for (i = 0; i < j->length; i++) {
			if (within == GEOM_POINT) {
				if (i == 0 || t == GEOM_MULTIPOINT) {
					op = VT_MOVETO;
				} else {
					op = VT_LINETO;
				}
			}

			parse_geometry(within, j->array[i], bbox, fpos, out, op);
		}
	} else {
		if (j->length == 2 && j->array[0]->type == JSON_NUMBER && j->array[1]->type == JSON_NUMBER) {
			unsigned x, y;
			double lon = j->array[0]->number;
			double lat = j->array[1]->number;
			latlon2tile(lat, lon, 32, &x, &y);

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

			serialize_int(out, op, fpos);
			serialize_uint(out, x, fpos);
			serialize_uint(out, y, fpos);
		} else {
			fprintf(stderr, "malformed point");
		}
	}

	if (t == GEOM_POLYGON) {
		serialize_int(out, VT_CLOSEPATH, fpos);
	}
}

void deserialize_int(char **f, int *n) {
	memcpy(n, *f, sizeof(int));
	*f += sizeof(int);
}

void deserialize_string(char **f) {
	int len;
	deserialize_int(f, &len);
	char s[len + 1];
	memcpy(s, *f, len);
	*f += len;
	s[len] = '\0';
	printf("%s", s);
}

void range_search(struct index *ix, long long n, unsigned long long start, unsigned long long end, struct index **pstart, struct index **pend) {
	struct index istart, iend;
	istart.index = start;
	iend.index = end;

	*pstart = search(&istart, ix, n, sizeof(struct index), indexcmp);
	*pend = search(&iend, ix, n, sizeof(struct index), indexcmp);

	if (*pend >= ix + n) {
		*pend = ix + n - 1;
	}
	while (*pstart > ix && indexcmp(*pstart - 1, &istart) == 0) {
		(*pstart)--;
	}
	if (indexcmp(*pstart, &istart) < 0) {
		(*pstart)++;
	}
	if (indexcmp(*pend, &iend) > 0) {
		(*pend)--;
	}
}

void check_range(struct index *start, struct index *end, char *metabase, unsigned *file_bbox) {
	struct index *i;

	for (i = start; i < end; i++) {
		printf("%llx ", i->index);

		char *meta = metabase + i->fpos;

		int m;
		deserialize_int(&meta, &m);

		int i;
		for (i = 0; i < m; i++) {
			int t;
			deserialize_int(&meta, &t);
			printf("(%d) ", t);
			deserialize_string(&meta);
			printf("=");
			deserialize_string(&meta);
			printf("; ");
		}

		int t;
		deserialize_int(&meta, &t);
		printf("(%d) ", t);

		while (1) {
			deserialize_int(&meta, &t);

			if (t == VT_END) {
				break;
			}

			printf("%d: ", t);

			if (t == VT_MOVETO || t == VT_LINETO) {
				int x, y;
				deserialize_int(&meta, &x);
				deserialize_int(&meta, &y);

				double lat, lon;
				tile2latlon(x, y, 32, &lat,&lon);
				printf("%f,%f (%x/%x) ", lat, lon, x, y);
			}
		}

		printf("\n");
	}
}

void check(struct index *ix, long long n, char *metabase, unsigned *file_bbox) {
	fprintf(stderr, "\n");

	int z;
	for (z = 11; z >= 11; z--) {
		struct index *i, *j = NULL;
		for (i = ix; i < ix + n && i != NULL; i = j) {
			unsigned wx, wy;
			decode(i->index, &wx, &wy);

			printf("%lld in %lld\n", (long long)(i - ix), (long long)n);

			for (j = i + 1; j < ix + n; j++) {
				unsigned wx2, wy2;
				decode(j->index, &wx2, &wy2);

				if (wx2 >> (32 - z) != wx >> (32 - z) || wy2 >> (32 - z) != wy >> (32 - z)) {
					break;
				}
			}

			printf("%d/%u/%u    %x %x   %lld to %lld\n", z, wx >> (32 - z), wy >> (32 - z), wx, wy, (long long)(i - ix), (long long)(j - ix));

			check_range(i, j, metabase, file_bbox);
		}
	}
}

void read_json(FILE *f) {
	char metaname[] = "/tmp/meta.XXXXXXXX";
	char indexname[] = "/tmp/index.XXXXXXXX";

	int metafd = mkstemp(metaname);
	int indexfd = mkstemp(indexname);

	FILE *metafile = fopen(metaname, "wb");
	FILE *indexfile = fopen(indexname, "wb");
	long long fpos = 0;

	unlink(metaname);
	unlink(indexname);

	unsigned file_bbox[] = { UINT_MAX, UINT_MAX, 0, 0 };

	json_pull *jp = json_begin_file(f);
	long long seq = 0;

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

		{
			long long start = fpos;

			char *metakey[properties->length];
			char *metaval[properties->length];
			int metatype[properties->length];
			int m = 0;

			int i;
			for (i = 0; i < properties->length; i++) {
				if (properties->keys[i]->type == JSON_STRING) {
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
						metaval[m] = properties->values[i]->string;
						m++;
					} else {
						fprintf(stderr, "%d: Unsupported metafile type\n", jp->line);
						goto next_feature;
					}
				}
			}

			serialize_int(metafile, m, &fpos);
			for (i = 0; i < m; i++) {
				serialize_int(metafile, metatype[i], &fpos);
				serialize_string(metafile, metakey[i], &fpos);
				serialize_string(metafile, metaval[i], &fpos);
			}

			unsigned bbox[] = { UINT_MAX, UINT_MAX, 0, 0 };

			serialize_int(metafile, t, &fpos);
			parse_geometry(t, coordinates, bbox, &fpos, metafile, VT_MOVETO);
			serialize_int(metafile, VT_END, &fpos);

			int z = 14;

			/* XXX do proper overlap instead of whole bounding box */
			unsigned x, y;
			for (x = bbox[0] >> (32 - z); x <= bbox[2] >> (32 - z); x++) {
				for (y = bbox[1] >> (32 - z); y <= bbox[3] >> (32 - z); y++) {
					struct index ix;
					ix.index = encode(x << (32 - z), y << (32 - z));
					ix.fpos = start;
					fwrite_check(&ix, sizeof(struct index), 1, indexfile);
				}
			}

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
			
			if (seq % 100000 == 0) {
				fprintf(stderr, "Read %.1f million features\r", seq / 1000000.0);
			}
			seq++;
		}

next_feature:
		json_free(j);

		/* XXX check for any non-features in the outer object */
	}

	json_end(jp);
	fclose(metafile);
	fclose(indexfile);

	printf("bbox: %x %x %x %x\n", file_bbox[0], file_bbox[1], file_bbox[2], file_bbox[3]);

	struct stat indexst;
	fstat(indexfd, &indexst);
	struct index *index = mmap(NULL, indexst.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, indexfd, 0);
	if (index == MAP_FAILED) {
		perror("mmap index");
		exit(EXIT_FAILURE);
	}

	struct stat metast;
	fstat(metafd, &metast);
	char *meta = mmap(NULL, metast.st_size, PROT_READ, MAP_PRIVATE, metafd, 0);
	if (meta == MAP_FAILED) {
		perror("mmap meta");
		exit(EXIT_FAILURE);
	}

	qsort(index, indexst.st_size / sizeof(struct index), sizeof(struct index), indexcmp);
	check(index, indexst.st_size / sizeof(struct index), meta, file_bbox);

	munmap(index, indexst.st_size);
	munmap(meta, metast.st_size);

	close(indexfd);
	close(metafd);
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
