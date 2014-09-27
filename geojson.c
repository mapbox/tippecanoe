#define _GNU_SOURCE
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
#include "jsonpull.h"
#include "tile.h"

int low_detail = 10;
int full_detail = 12;

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

struct pool_val *pool1(struct pool *p, char *s, int type, int (*compare)(const char *, const char *)) {
	struct pool_val **v = &(p->vals);

	while (*v != NULL) {
		int cmp = compare(s, (*v)->s);

		if (cmp == 0) {
			cmp = type - (*v)->type;
		}

		if (cmp == 0) {
			return *v;
		} else if (cmp < 0) {
			v = &((*v)->left);
		} else {
			v = &((*v)->right);
		}
	}

	*v = malloc(sizeof(struct pool_val));
	(*v)->left = NULL;
	(*v)->right = NULL;
	(*v)->next = NULL;
	(*v)->s = s;
	(*v)->type = type;
	(*v)->n = p->n++;

	if (p->tail != NULL) {
		p->tail->next = *v;
	}
	p->tail = *v;
	if (p->head == NULL) {
		p->head = *v;
	}

	return *v;
}

struct pool_val *pool(struct pool *p, char *s, int type) {
	return pool1(p, s, type, strcmp);
}

int llcmp(const char *v1, const char *v2) {
	long long *ll1 = (long long *) v1;
	long long *ll2 = (long long *) v2;

	if (*ll1 < *ll2) {
		return -1;
	} else if (*ll1 > *ll2) {
		return 1;
	} else {
		return 0;
	}
}

struct pool_val *pool_long_long(struct pool *p, long long *s, int type) {
	return pool1(p, (char *) s, type, llcmp);
}

void pool_free(struct pool *p) {
	while (p->head != NULL) {
		struct pool_val *next = p->head->next;
		free(p->head);
		p->head = next;
	}

	p->head = NULL;
	p->tail = NULL;
	p->vals = NULL;
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

	serialize_int(out, len + 1, fpos);
	fwrite_check(s, sizeof(char), len, out);
	fwrite_check("", sizeof(char), 1, out);
	*fpos += len + 1;
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
				if (i == 0 || mb_geometry[t] == GEOM_MULTIPOINT) {
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

	if (mb_geometry[t] == GEOM_POLYGON) {
		serialize_int(out, VT_CLOSEPATH, fpos);
	}
}

void deserialize_int(char **f, int *n) {
	memcpy(n, *f, sizeof(int));
	*f += sizeof(int);
}

struct pool_val *deserialize_string(char **f, struct pool *p, int type) {
	struct pool_val *ret;
	int len;

	deserialize_int(f, &len);
	ret = pool(p, *f, type);
	*f += len;

	return ret;
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

void check(struct index *ix, long long n, char *metabase, unsigned *file_bbox, struct pool *file_keys, unsigned *midx, unsigned *midy, char *layername, int maxzoom, int minzoom, sqlite3 *outdb) {
	fprintf(stderr, "\n");
	long long most = 0;

	int z;
	for (z = maxzoom; z >= minzoom; z--) {
		struct index *i, *j = NULL;
		for (i = ix; i < ix + n && i != NULL; i = j) {
			unsigned wx, wy;
			decode(i->index, &wx, &wy);

			unsigned tx = 0, ty = 0;
			if (z != 0) {
				tx = wx >> (32 - z);
				ty = wy >> (32 - z);
			}

			// printf("%lld in %lld\n", (long long)(i - ix), (long long)n);

			for (j = i + 1; j < ix + n; j++) {
				unsigned wx2, wy2;
				decode(j->index, &wx2, &wy2);

				unsigned tx2 = 0, ty2 = 0;
				if (z != 0) {
					tx2 = wx2 >> (32 - z);
					ty2 = wy2 >> (32 - z);
				}

				if (tx2 != tx || ty2 != ty) {
					break;
				}
			}

			fprintf(stderr, "  %3.1f%%   %d/%u/%u    %x %x   %lld to %lld    \r", (((i - ix) + (j - ix)) / 2.0 / n + (maxzoom - z)) / (maxzoom - minzoom + 1) * 100, z, tx, ty, wx, wy, (long long)(i - ix), (long long)(j - ix));

			long long len = write_tile(i, j, metabase, file_bbox, z, tx, ty, z == maxzoom ? full_detail : low_detail, maxzoom, file_keys, layername, outdb);

			if (z == maxzoom && len > most) {
				*midx = tx;
				*midy = ty;
				most = len;
			}
		}
	}

	fprintf(stderr, "\n");
}

void quote(char **buf, char *s) {
	char tmp[strlen(s) * 8 + 1];
	char *out = tmp;

	for (; *s != '\0'; s++) {
		if (*s == '\\' || *s == '\"') {
			*out++ = '\\';
			*out++ = *s;
		} else if (*s < ' ') {
			sprintf(out, "\\u%04x", *s);
			out = out + strlen(out);
		} else {
			*out++ = *s;
		}
	}

	*out = '\0';
	*buf = realloc(*buf, strlen(*buf) + strlen(tmp) + 1);
	strcat(*buf, tmp);
}

void aprintf(char **buf, const char *format, ...) {
	va_list ap;
	char *tmp;

	va_start(ap, format);
	if (vasprintf(&tmp, format, ap) < 0) {
		fprintf(stderr, "memory allocation failure\n");
		exit(EXIT_FAILURE);
	}
	va_end(ap);

	*buf = realloc(*buf, strlen(*buf) + strlen(tmp) + 1);
	strcat(*buf, tmp);
	free(tmp);
}

void read_json(FILE *f, char *fname, char *layername, int maxzoom, int minzoom, sqlite3 *outdb) {
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
	unsigned midx = 0, midy = 0;

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

			unsigned bbox[] = { UINT_MAX, UINT_MAX, 0, 0 };

			serialize_int(metafile, mb_geometry[t], &fpos);
			parse_geometry(t, coordinates, bbox, &fpos, metafile, VT_MOVETO);
			serialize_int(metafile, VT_END, &fpos);

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

			int z = 14;

			unsigned cx = bbox[0] / 2 + bbox[2] / 2;
			unsigned cy = bbox[1] / 2 + bbox[3] / 2;

			/* XXX do proper overlap instead of whole bounding box */
			if (z == 0) {
				struct index ix;
				ix.index = encode(cx, cy);
				ix.fpos = start;
				fwrite_check(&ix, sizeof(struct index), 1, indexfile);
			} else {
				unsigned x, y;
				for (x = bbox[0] >> (32 - z); x <= bbox[2] >> (32 - z); x++) {
					for (y = bbox[1] >> (32 - z); y <= bbox[3] >> (32 - z); y++) {
						struct index ix;

						if (x == cx >> (32 - z) && y == cy >> (32 - z)) {
							ix.index = encode(cx, cy);
						} else {
							ix.index = encode(x << (32 - z), y << (32 - z));
						}
						ix.fpos = start;
						fwrite_check(&ix, sizeof(struct index), 1, indexfile);
					}
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

	struct pool file_keys;
	file_keys.n = 0;
	file_keys.vals = NULL;
	file_keys.head = NULL;
	file_keys.tail = NULL;

	char trunc[strlen(fname) + 1];
	if (layername == NULL) {
		char *cp, *use = fname;
		for (cp = fname; *cp; cp++) {
			if (*cp == '/' && cp[1] != '\0') {
				use = cp + 1;
			}
		}
		strcpy(trunc, use);
		cp = strstr(trunc, ".json");
		if (cp != NULL) {
			*cp = '\0';
		}
		layername = trunc;

		char *out = trunc;
		for (cp = trunc; *cp; cp++) {
			if (isalpha(*cp) || isdigit(*cp) || *cp == '_') {
				*out++ = *cp;
			}
		}
		*out = '\0';

		printf("using layer name %s\n", trunc);
	}

	qsort(index, indexst.st_size / sizeof(struct index), sizeof(struct index), indexcmp);
	check(index, indexst.st_size / sizeof(struct index), meta, file_bbox, &file_keys, &midx, &midy, layername, maxzoom, minzoom, outdb);

	munmap(index, indexst.st_size);
	munmap(meta, metast.st_size);

	close(indexfd);
	close(metafd);

	{
		char *sql, *err;

		sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('name', %Q);", fname);
		if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "set name in metadata: %s\n", err);
			exit(EXIT_FAILURE);
		}
		sqlite3_free(sql);

		sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('description', %Q);", fname);
		if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "set description in metadata: %s\n", err);
			exit(EXIT_FAILURE);
		}
		sqlite3_free(sql);

		sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('version', %d);", 1);
		if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "set metadata: %s\n", err);
			exit(EXIT_FAILURE);
		}
		sqlite3_free(sql);

		sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('minzoom', %d);", minzoom);
		if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "set metadata: %s\n", err);
			exit(EXIT_FAILURE);
		}
		sqlite3_free(sql);

		sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('maxzoom', %d);", maxzoom);
		if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "set metadata: %s\n", err);
			exit(EXIT_FAILURE);
		}
		sqlite3_free(sql);

		double minlat = 0, minlon = 0, maxlat = 0, maxlon = 0, midlat = 0, midlon = 0;

		tile2latlon(midx, midy, maxzoom, &maxlat, &minlon);
		tile2latlon(midx + 1, midy + 1, maxzoom, &minlat, &maxlon);

		midlat = (maxlat + minlat) / 2;
		midlon = (maxlon + minlon) / 2;

		tile2latlon(file_bbox[0], file_bbox[1], 32, &maxlat, &minlon);
		tile2latlon(file_bbox[2], file_bbox[3], 32, &minlat, &maxlon);

		sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('center', '%f,%f,%d');", midlon, midlat, maxzoom);
		if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "set metadata: %s\n", err);
			exit(EXIT_FAILURE);
		}
		sqlite3_free(sql);

		sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('bounds', '%f,%f,%f,%f');", minlon, minlat, maxlon, maxlat);
		if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "set metadata: %s\n", err);
			exit(EXIT_FAILURE);
		}
		sqlite3_free(sql);

		sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('type', %Q);", "overlay");
		if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "set metadata: %s\n", err);
			exit(EXIT_FAILURE);
		}
		sqlite3_free(sql);

		sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('format', %Q);", "pbf");
		if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "set metadata: %s\n", err);
			exit(EXIT_FAILURE);
		}
		sqlite3_free(sql);

		char *buf = strdup("{");
		aprintf(&buf, "\"vector_layers\": [ { \"id\": \"");
		quote(&buf, layername);
		aprintf(&buf, "\", \"description\": \"\", \"minzoom\": %d, \"maxzoom\": %d, \"fields\": {", minzoom, maxzoom);

		struct pool_val *pv;
		for (pv = file_keys.head; pv != NULL; pv = pv->next) {
			aprintf(&buf, "\"");
			quote(&buf, pv->s);

			if (pv->type == VT_NUMBER) { 
				aprintf(&buf, "\": \"Number\"");
			} else {
				aprintf(&buf, "\": \"String\"");
			}

			if (pv->next != NULL) {
				aprintf(&buf, ", ");
			}
		}

		aprintf(&buf, "} } ] }");

		sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('json', %Q);", buf);
		if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "set metadata: %s\n", err);
			exit(EXIT_FAILURE);
		}
		sqlite3_free(sql);
	}
}

int main(int argc, char **argv) {
	extern int optind;
	extern char *optarg;
	int i;

	char *name = NULL;
	char *layer = NULL;
	char *outdir = NULL;
	int maxzoom = 14;
	int minzoom = 0;

	while ((i = getopt(argc, argv, "l:n:z:Z:d:D:o:")) != -1) {
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

		case 'd':
			full_detail = atoi(optarg);
			break;

		case 'D':
			low_detail = atoi(optarg);
			break;

		case 'o':
			outdir = optarg;
			break;

		default:
			fprintf(stderr, "Usage: %s -o out.mbtiles [-n name] [-l layername] [-z maxzoom] [-Z minzoom] [-d detail] [-D lower-detail] [file.json]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (outdir == NULL) {
		fprintf(stderr, "%s: must specify -o out.mbtiles\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	sqlite3 *outdb;
	if (sqlite3_open(outdir, &outdb) != SQLITE_OK) {
		fprintf(stderr, "%s: %s: %s\n", argv[0], outdir,  sqlite3_errmsg(outdb));
		exit(EXIT_FAILURE);
	}

	char *err = NULL;
	if (sqlite3_exec(outdb, "PRAGMA synchronous=0", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: async: %s\n", argv[0], err);
		exit(EXIT_FAILURE);
	}
	if (sqlite3_exec(outdb, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: async: %s\n", argv[0], err);
		exit(EXIT_FAILURE);
	}
	if (sqlite3_exec(outdb, "PRAGMA journal_mode=DELETE", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: async: %s\n", argv[0], err);
		exit(EXIT_FAILURE);
	}
	if (sqlite3_exec(outdb, "CREATE TABLE metadata (name text, value text);", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: create metadata table: %s\n", argv[0], err);
		exit(EXIT_FAILURE);
	}
	if (sqlite3_exec(outdb, "CREATE TABLE tiles (zoom_level integer, tile_column integer, tile_row integer, tile_data blob);", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: create tiles table: %s\n", argv[0], err);
		exit(EXIT_FAILURE);
	}
	if (sqlite3_exec(outdb, "create unique index name on metadata (name);", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: index metadata: %s\n", argv[0], err);
		exit(EXIT_FAILURE);
	}
	if (sqlite3_exec(outdb, "create unique index tile_index on tiles (zoom_level, tile_column, tile_row);", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: index tiles: %s\n", argv[0], err);
		exit(EXIT_FAILURE);
	}
	
	if (argc == optind + 1) {
		int i;
		for (i = optind; i < argc; i++) {
			FILE *f = fopen(argv[i], "r");
			if (f == NULL) {
				fprintf(stderr, "%s: %s: %s\n", argv[0], argv[i], strerror(errno));
			} else {
				read_json(f, name ? name : argv[i], layer, maxzoom, minzoom, outdb);
				fclose(f);
			}
		}
	} else if (argc > optind) {
		fprintf(stderr, "%s: Only accepts one input file\n", argv[0]);
		exit(EXIT_FAILURE);
	} else {
		read_json(stdin, name ? name : "standard input", layer, maxzoom, minzoom, outdb);
	}

	if (sqlite3_exec(outdb, "ANALYZE;", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: index metadata: %s\n", argv[0], err);
		exit(EXIT_FAILURE);
	}
	if (sqlite3_exec(outdb, "VACUUM;", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: index tiles: %s\n", argv[0], err);
		exit(EXIT_FAILURE);
	}
	if (sqlite3_close(outdb) != SQLITE_OK) {
		fprintf(stderr, "%s: could not close database: %s\n", argv[0], sqlite3_errmsg(outdb));
		exit(EXIT_FAILURE);
	}

	return 0;
}
