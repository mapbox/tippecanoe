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

#define MAX_ZOOM 28
#define ZOOM_BITS 5

int get_bbox_zoom(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2) {
	int z;
	for (z = 0; z < MAX_ZOOM; z++) {
		int mask = 1 << (32 - (z + 1));

		if (((x1 & mask) != (x2 & mask)) ||
		    ((y1 & mask) != (y2 & mask))) {
			return z;
		}
	}

	return MAX_ZOOM;
}

void get_bbox_tile(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, int *z, unsigned int *x, unsigned int *y) {
	*z = get_bbox_zoom(x1, y1, x2, y2);

	if (*z == 0) {
		*x = *y = 0;
	} else {
		*x = x1 >> (32 - *z);
		*y = y1 >> (32 - *z);
	}
}

/*
 *  5 bits for zoom             (<< 59)
 * 56 bits for interspersed yx  (<< 3)
 *  3 bits for tags             (<< 0)
 */
unsigned long long encode_bbox(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, int tags) {
	int z = get_bbox_zoom(x1, y1, x2, y2);
	long long out = ((long long) z) << (64 - ZOOM_BITS);

	int i;
	for (i = 0; i < MAX_ZOOM; i++) {
		long long v = ((y1 >> (32 - (i + 1))) & 1) << 1;
		v |= (x1 >> (32 - (i + 1))) & 1;
		v = v << (64 - ZOOM_BITS - 2 * (i + 1));

		out |= v;
	}

	return out;
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

void check(struct index *ix, long long n, char *metabase) {
	int i;

	for (i = 0; i < n; i++) {
		printf("%llx ", ix[i].index);

		char *meta = metabase + ix[i].fpos;

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

				printf("%x,%x ", x, y);
			}
		}

		printf("\n");
	}
}

void read_json(FILE *f) {
	json_pull *jp = json_begin_file(f);

	FILE *metafile = fopen("meta.out", "wb");
	FILE *indexfile = fopen("index.out", "wb");
	long long fpos = 0;

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
			
			// printf("bbox %x %x %x %x\n", bbox[0], bbox[1], bbox[2], bbox[3]);

			struct index ix;
			ix.index = encode_bbox(bbox[0], bbox[1], bbox[2], bbox[3], 0);
			ix.fpos = start;
			fwrite_check(&ix, sizeof(struct index), 1, indexfile);
		}

next_feature:
		json_free(j);

		/* XXX check for any non-features in the outer object */
	}

	json_end(jp);
	fclose(metafile);
	fclose(indexfile);

	int fd = open("index.out", O_RDWR);
	struct stat st;
	fstat(fd, &st);
	struct index *index = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (index == MAP_FAILED) {
		perror("mmap index");
		exit(EXIT_FAILURE);
	}

	int mfd = open("meta.out", O_RDWR);
	struct stat mst;
	fstat(mfd, &mst);
	char *meta = mmap(NULL, mst.st_size, PROT_READ, MAP_PRIVATE, mfd, 0);
	if (meta == MAP_FAILED) {
		perror("mmap meta");
		exit(EXIT_FAILURE);
	}

	qsort(index, st.st_size / sizeof(struct index), sizeof(struct index), indexcmp);
	check(index, st.st_size / sizeof(struct index), meta);

	munmap(index, st.st_size);
	munmap(meta, mst.st_size);
	close(fd);
	close(mfd);
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
