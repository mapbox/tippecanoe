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
#include "pool.h"
#include "mbtiles.h"
#include "projection.h"

int low_detail = 10;
int full_detail = -1;

#define GEOM_POINT 0                /* array of positions */
#define GEOM_MULTIPOINT 1           /* array of arrays of positions */
#define GEOM_LINESTRING 2           /* array of arrays of positions */
#define GEOM_MULTILINESTRING 3      /* array of arrays of arrays of positions */
#define GEOM_POLYGON 4              /* array of arrays of arrays of positions */
#define GEOM_MULTIPOLYGON 5         /* array of arrays of arrays of arrays of positions */
#define GEOM_TYPES 6

const char *geometry_names[GEOM_TYPES] = {
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

int indexcmp(const void *v1, const void *v2) {
	const struct index *i1 = (const struct index *) v1;
	const struct index *i2 = (const struct index *) v2;

	if (i1->index < i2->index) {
		return -1;
	} else if (i1->index > i2->index) {
		return 1;
	}

	if (i1->fpos < i2->fpos) {
		return -1;
	} else if (i1->fpos > i2->fpos) {
		return 1;
	}

	if (i1->maxzoom < i2->maxzoom) {
		return -1;
	} else if (i1->maxzoom > i2->maxzoom) {
		return 1;
	}

	return 0;
}

size_t fwrite_check(const void *ptr, size_t size, size_t nitems, FILE *stream, const char *fname, json_pull *source) {
	size_t w = fwrite(ptr, size, nitems, stream);
	if (w != nitems) {
		fprintf(stderr, "%s:%d: Write to temporary file failed: %s\n", fname, source->line, strerror(errno));
		exit(EXIT_FAILURE);
	}
	return w;
}

void serialize_int(FILE *out, int n, long long *fpos, const char *fname, json_pull *source) {
	fwrite_check(&n, sizeof(int), 1, out, fname, source);
	*fpos += sizeof(int);
}

void serialize_long_long(FILE *out, long long n, long long *fpos, const char *fname, json_pull *source) {
	fwrite_check(&n, sizeof(long long), 1, out, fname, source);
	*fpos += sizeof(long long);
}

void serialize_byte(FILE *out, signed char n, long long *fpos, const char *fname, json_pull *source) {
	fwrite_check(&n, sizeof(signed char), 1, out, fname, source);
	*fpos += sizeof(signed char);
}

void serialize_uint(FILE *out, unsigned n, long long *fpos, const char *fname, json_pull *source) {
	fwrite_check(&n, sizeof(unsigned), 1, out, fname, source);
	*fpos += sizeof(unsigned);
}

void serialize_string(FILE *out, const char *s, long long *fpos, const char *fname, json_pull *source) {
	int len = strlen(s);

	serialize_int(out, len + 1, fpos, fname, source);
	fwrite_check(s, sizeof(char), len, out, fname, source);
	fwrite_check("", sizeof(char), 1, out, fname, source);
	*fpos += len + 1;
}

void parse_geometry(int t, json_object *j, unsigned *bbox, long long *fpos, FILE *out, int op, const char *fname, json_pull *source) {
	if (j == NULL || j->type != JSON_ARRAY) {
		fprintf(stderr, "%s:%d: expected array for type %d\n", fname, source->line, t);
		return;
	}

	int within = geometry_within[t];
	long long began = *fpos;
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

			parse_geometry(within, j->array[i], bbox, fpos, out, op, fname, source);
		}
	} else {
		if (j->length >= 2 && j->array[0]->type == JSON_NUMBER && j->array[1]->type == JSON_NUMBER) {
			unsigned x, y;
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

			serialize_byte(out, op, fpos, fname, source);
			serialize_uint(out, x, fpos, fname, source);
			serialize_uint(out, y, fpos, fname, source);
		} else {
			fprintf(stderr, "%s:%d: malformed point\n", fname, source->line);
		}
	}

	if (t == GEOM_POLYGON) {
		if (*fpos != began) {
			serialize_byte(out, VT_CLOSEPATH, fpos, fname, source);
		}
	}
}

void deserialize_int(char **f, int *n) {
	memcpy(n, *f, sizeof(int));
	*f += sizeof(int);
}

void deserialize_long_long(char **f, long long *n) {
	memcpy(n, *f, sizeof(long long));
	*f += sizeof(long long);
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

void check(int geomfd[4], off_t geom_size[4], char *metabase, unsigned *file_bbox, struct pool *file_keys, unsigned *midx, unsigned *midy, const char *layername, int maxzoom, int minzoom, sqlite3 *outdb, double droprate, int buffer, const char *fname, struct json_pull *jp, const char *tmpdir) {
	int i;
	for (i = 0; i <= maxzoom; i++) {
		long long most = 0;

		FILE *sub[4];
		int subfd[4];
		int j;
		for (j = 0; j < 4; j++) {
			char geomname[strlen(tmpdir) + strlen("/geom2.XXXXXXXX") + 1];
			sprintf(geomname, "%s/geom%d.XXXXXXXX", tmpdir, j);
			subfd[j] = mkstemp(geomname);
			//printf("%s\n", geomname);
			if (subfd[j] < 0) {
				perror(geomname);
				exit(EXIT_FAILURE);
			}
			sub[j] = fopen(geomname, "wb");
			if (sub[j] == NULL) {
				perror(geomname);
				exit(EXIT_FAILURE);
			}
			unlink(geomname);
		}

		long long todo = 0;
		long long along = 0;
		for (j = 0; j < 4; j++) {
			todo += geom_size[j];
		}

		for (j = 0; j < 4; j++) {
			if (geomfd[j] < 0) {
				// only one source file for zoom level 0
				continue;
			}
			if (geom_size[j] == 0) {
				continue;
			}

			// printf("%lld of geom_size\n", (long long) geom_size[j]);

			char *geom = mmap(NULL, geom_size[j], PROT_READ, MAP_PRIVATE, geomfd[j], 0);
			if (geom == MAP_FAILED) {
				perror("mmap geom");
				exit(EXIT_FAILURE);
			}

			char *geomstart = geom;
			char *end = geom + geom_size[j];

			while (geom < end) {
				int z;
				unsigned x, y;

				deserialize_int(&geom, &z);
				deserialize_uint(&geom, &x);
				deserialize_uint(&geom, &y);

				// fprintf(stderr, "%d/%u/%u\n", z, x, y);

				fprintf(stderr, "  %3.1f%%   %d/%u/%u  \r",
					(((geom - geomstart + along) / (double) todo) + z) / (maxzoom + 1) * 100, z, x, y);

				long long len = write_tile(&geom, metabase, file_bbox, z, x, y, z == maxzoom ? full_detail : low_detail, maxzoom, file_keys, layername, outdb, droprate, buffer, fname, jp, sub);

				if (z == maxzoom && len > most) {
					*midx = x;
					*midy = y;
					most = len;
				}
			}

			munmap(geom, geom_size[j]);
			along += geom_size[j];
		}

		for (j = 0; j < 4; j++) {
			close(geomfd[j]);
			fclose(sub[j]);

			struct stat geomst;
			if (fstat(subfd[j], &geomst) != 0) {
				perror("stat geom\n");
				exit(EXIT_FAILURE);
			}

			geomfd[j] = subfd[j];
			geom_size[j] = geomst.st_size;
		}
	}

	fprintf(stderr, "\n");
}

struct merge {
	long long start;
	long long end;

	struct merge *next;
};

#if 0
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
		fwrite(map + head->start, bytes, 1, f);
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
			fprintf(stderr, "Merging: %lld%%\r", report);
			reported = report;
		}
	}
}
#endif

void read_json(FILE *f, const char *fname, const char *layername, int maxzoom, int minzoom, sqlite3 *outdb, struct pool *exclude, struct pool *include, int exclude_all, double droprate, int buffer, const char *tmpdir) {
	char metaname[strlen(tmpdir) + strlen("/meta.XXXXXXXX") + 1];
	char geomname[strlen(tmpdir) + strlen("/geom.XXXXXXXX") + 1];

	sprintf(metaname, "%s%s", tmpdir, "/meta.XXXXXXXX");
	sprintf(geomname, "%s%s", tmpdir, "/geom.XXXXXXXX");

	int metafd = mkstemp(metaname);
	if (metafd < 0) {
		perror(metaname);
		exit(EXIT_FAILURE);
	}
	int geomfd = mkstemp(geomname);
	printf("%s\n", geomname);
	if (geomfd < 0) {
		perror(geomname);
		exit(EXIT_FAILURE);
	}

	FILE *metafile = fopen(metaname, "wb");
	if (metafile == NULL) {
		perror(metaname);
		exit(EXIT_FAILURE);
	}
	FILE *geomfile = fopen(geomname, "wb");
	if (geomfile == NULL) {
		perror(geomname);
		exit(EXIT_FAILURE);
	}
	long long metapos = 0;
	long long geompos = 0;

	unlink(metaname);
	unlink(geomname);

	unsigned file_bbox[] = { UINT_MAX, UINT_MAX, 0, 0 };
	unsigned midx = 0, midy = 0;

	json_pull *jp = json_begin_file(f);
	long long seq = 0;

	/* initial tile is 0/0/0 */
	serialize_int(geomfile, 0, &geompos, fname, jp);
	serialize_uint(geomfile, 0, &geompos, fname, jp);
	serialize_uint(geomfile, 0, &geompos, fname, jp);

	while (1) {
		json_object *j = json_read(jp);
		if (j == NULL) {
			if (jp->error != NULL) {
				fprintf(stderr, "%s:%d: %s\n", fname, jp->line, jp->error);
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
			fprintf(stderr, "%s:%d: feature with no geometry\n", fname, jp->line);
			json_free(j);
			continue;
		}

		json_object *geometry_type = json_hash_get(geometry, "type");
		if (geometry_type == NULL) {
			static int warned = 0;
			if (!warned) {
				fprintf(stderr, "%s:%d: null geometry (additional not reported)\n", fname, jp->line);
				warned = 1;
			}

			json_free(j);
			continue;
		}

		if (geometry_type->type != JSON_STRING) {
			fprintf(stderr, "%s:%d: geometry without type\n", fname, jp->line);
			json_free(j);
			continue;
		}

		json_object *properties = json_hash_get(j, "properties");
		if (properties == NULL || properties->type != JSON_HASH) {
			fprintf(stderr, "%s:%d: feature without properties hash\n", fname, jp->line);
			json_free(j);
			continue;
		}

		json_object *coordinates = json_hash_get(geometry, "coordinates"); 
		if (coordinates == NULL || coordinates->type != JSON_ARRAY) {
			fprintf(stderr, "%s:%d: feature without coordinates array\n", fname, jp->line);
			json_free(j);
			continue;
		}

		int t;
		for (t = 0; t < GEOM_TYPES; t++) {
			if (strcmp(geometry_type->string, geometry_names[t]) == 0) {
				break;
			}
		}
		if (t >= GEOM_TYPES) {
			fprintf(stderr, "%s:%d: Can't handle geometry type %s\n", fname, jp->line, geometry_type->string);
			json_free(j);
			continue;
		}

		{
			unsigned bbox[] = { UINT_MAX, UINT_MAX, 0, 0 };

			long long metastart = metapos;
			char *metakey[properties->length];
			char *metaval[properties->length];
			int metatype[properties->length];
			int m = 0;

			int i;
			for (i = 0; i < properties->length; i++) {
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
						metaval[m] = properties->values[i]->string;
						m++;
					} else if (properties->values[i] != NULL && (properties->values[i]->type == JSON_NULL)) {
						;
					} else {
						fprintf(stderr, "%s:%d: Unsupported property type for %s\n", fname, jp->line, properties->keys[i]->string);
						json_free(j);
						continue;
					}
				}
			}

			serialize_int(metafile, m, &metapos, fname, jp);
			for (i = 0; i < m; i++) {
				serialize_int(metafile, metatype[i], &metapos, fname, jp);
				serialize_string(metafile, metakey[i], &metapos, fname, jp);
				serialize_string(metafile, metaval[i], &metapos, fname, jp);
			}

			serialize_int(geomfile, mb_geometry[t], &geompos, fname, jp);
			serialize_long_long(geomfile, metastart, &geompos, fname, jp);
			parse_geometry(t, coordinates, bbox, &geompos, geomfile, VT_MOVETO, fname, jp);
			serialize_byte(geomfile, VT_END, &geompos, fname, jp);

#if 0
			int z = maxzoom;
#endif

			/*
			 * Note that minzoom for lines is the dimension
			 * of the geometry in world coordinates, but
			 * for points is the lowest zoom level (in tiles,
			 * not in pixels) at which it should be drawn.
			 *
			 * So a line that is too small for, say, z8
			 * will have minzoom of 18 (if tile detail is 10),
			 * not 8.
			 */
			int minzoom = 0;
			if (mb_geometry[t] == VT_LINE) {
				for (minzoom = 0; minzoom < 31; minzoom++) {
					unsigned mask = 1 << (32 - (minzoom + 1));

					if (((bbox[0] & mask) != (bbox[2] & mask)) ||
					    ((bbox[1] & mask) != (bbox[3] & mask))) {
						break;
					}
				}
			} else if (mb_geometry[t] == VT_POINT) {
				double r = ((double) rand()) / RAND_MAX;
				if (r == 0) {
					r = .00000001;
				}
				minzoom = maxzoom - floor(log(r) / - log(droprate));
			}

			serialize_byte(geomfile, minzoom, &geompos, fname, jp);

#if 0
			unsigned cx = bbox[0] / 2 + bbox[2] / 2;
			unsigned cy = bbox[1] / 2 + bbox[3] / 2;

			/* XXX do proper overlap instead of whole bounding box */
			if (z == 0) {
				struct index ix;
				ix.index = encode(cx, cy);
				ix.fpos = start;
				ix.type = mb_geometry[t];
				ix.maxzoom = z;
				ix.minzoom = minzoom;
				fwrite_check(&ix, sizeof(struct index), 1, indexfile, fname, jp);
			} else {
				int pass;
				int instances = 0;

				for (pass = 0; pass < 2; pass++) {
					for (z = maxzoom; z >= 1; z--) {
						unsigned x, y;
						for (x = (bbox[0] - (buffer << (32 - z - 8))) >> (32 - z); x <= (bbox[2] + (buffer << (32 - z - 8))) >> (32 - z); x++) {
							for (y = (bbox[1] - (buffer << (32 - z - 8))) >> (32 - z); y <= (bbox[3] + (buffer << (32 - z - 8))) >> (32 - z); y++) {
								if (z != maxzoom) {
									// There must be a clearer way to write this, but the intent is
									// not to add an additional index for a low-zoom tile
									// if one of its children was already part of the
									// buffered bounding box for the child's zoom.

									// So we are comparing this tile's x and y to the edges of the
									// bounding box at the next zoom down, but divided by two
									// to get it back into this zoom's tile coordinate scheme

									if ((x >= ((bbox[0] - (buffer << (32 - (z + 1) - 8))) >> (32 - (z + 1)) >> 1)) &&
									    (x <= ((bbox[2] + (buffer << (32 - (z + 1) - 8))) >> (32 - (z + 1)) >> 1)) &&
									    (y >= ((bbox[1] - (buffer << (32 - (z + 1) - 8))) >> (32 - (z + 1)) >> 1)) &&
									    (y <= ((bbox[3] + (buffer << (32 - (z + 1) - 8))) >> (32 - (z + 1)) >> 1))) {
										continue;
									}
								}

								if (pass == 0) {
									instances++;
									if (instances > 1) {
										break;
									}
								} else {
									struct index ix;

									if (x == cx >> (32 - z) && y == cy >> (32 - z)) {
										ix.index = encode(cx, cy);
									} else {
										ix.index = encode(x << (32 - z), y << (32 - z));
									}
									ix.fpos = start;
									ix.type = mb_geometry[t];
									ix.maxzoom = z;
									ix.candup = (instances > 1);
									ix.minzoom = minzoom;
									fwrite_check(&ix, sizeof(struct index), 1, indexfile, fname, jp);
								}
							}
						}
					}
				}
			}
#endif

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
			
			if (seq % 10000 == 0) {
				fprintf(stderr, "Read %.2f million features\r", seq / 1000000.0);
			}
			seq++;
		}

		json_free(j);

		/* XXX check for any non-features in the outer object */
	}

	/* end of tile */
	serialize_int(geomfile, -2, &geompos, fname, jp);

	json_end(jp);
	fclose(metafile);
	fclose(geomfile);

	printf("bbox: %x %x %x %x\n", file_bbox[0], file_bbox[1], file_bbox[2], file_bbox[3]);

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
		fprintf(stderr, "%s: did not read any valid geometries\n", fname);
		exit(EXIT_FAILURE);
	}

	char *meta = (char *) mmap(NULL, metast.st_size, PROT_READ, MAP_PRIVATE, metafd, 0);
	if (meta == MAP_FAILED) {
		perror("mmap meta");
		exit(EXIT_FAILURE);
	}

	struct pool file_keys;
	pool_init(&file_keys, 0);

	char trunc[strlen(fname) + 1];
	if (layername == NULL) {
		const char *ocp, *use = fname;
		for (ocp = fname; *ocp; ocp++) {
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

#if 0
	{
		int bytes = sizeof(struct index);

		fprintf(stderr,
		 	"Sorting %lld indices for %lld features\n",
			(long long) indexst.st_size / bytes,
			seq);

		int page = sysconf(_SC_PAGESIZE);
		long long unit = (50 * 1024 * 1024 / bytes) * bytes;
		while (unit % page != 0) {
			unit += bytes;
		}

		int nmerges = (indexst.st_size + unit - 1) / unit;
		struct merge merges[nmerges];

		long long start;
		for (start = 0; start < indexst.st_size; start += unit) {
			long long end = start + unit;
			if (end > indexst.st_size) {
				end = indexst.st_size;
			}

			if (nmerges != 1) {
				fprintf(stderr, "Sorting part %lld of %d\r", start / unit + 1, nmerges);
			}

			merges[start / unit].start = start;
			merges[start / unit].end = end;
			merges[start / unit].next = NULL;

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
			fprintf(stderr, "\n");
		}

		void *map = mmap(NULL, indexst.st_size, PROT_READ, MAP_PRIVATE, indexfd, 0);
		if (map == MAP_FAILED) {
			perror("mmap");
			exit(EXIT_FAILURE);
		}

		FILE *f = fopen(indexname, "w");
		if (f == NULL) {
			perror(indexname);
			exit(EXIT_FAILURE);
		}

		merge(merges, nmerges, (unsigned char *) map, f, bytes, indexst.st_size / bytes);

		munmap(map, indexst.st_size);
		fclose(f);
		close(indexfd);
	}
#endif

	int fd[4];
	off_t size[4];

	fd[0] = geomfd;
	size[0] = geomst.st_size;

	int j;
	for (j = 1; j < 4; j++) {
		fd[j] = -1;
		size[j] = 0;
	}

	fprintf(stderr, "%lld features, %lld bytes of geometry, %lld bytes of metadata\n", seq, (long long) geomst.st_size, (long long) metast.st_size);

	check(fd, size, meta, file_bbox, &file_keys, &midx, &midy, layername, maxzoom, minzoom, outdb, droprate, buffer, fname, jp, tmpdir);

	munmap(meta, metast.st_size);

	close(geomfd);
	close(metafd);


	double minlat = 0, minlon = 0, maxlat = 0, maxlon = 0, midlat = 0, midlon = 0;

	tile2latlon(midx, midy, maxzoom, &maxlat, &minlon);
	tile2latlon(midx + 1, midy + 1, maxzoom, &minlat, &maxlon);

	midlat = (maxlat + minlat) / 2;
	midlon = (maxlon + minlon) / 2;

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

	mbtiles_write_metadata(outdb, fname, layername, minzoom, maxzoom, minlat, minlon, maxlat, maxlon, midlat, midlon, &file_keys);

	pool_free_strings(&file_keys);
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
	int force = 0;
	double droprate = 2.5;
	int buffer = 5;
	const char *tmpdir = "/tmp";

	struct pool exclude, include;
	pool_init(&exclude, 0);
	pool_init(&include, 0);
	int exclude_all = 0;

	while ((i = getopt(argc, argv, "l:n:z:Z:d:D:o:x:y:r:b:fXt:")) != -1) {
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
			droprate = atof(optarg);
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

		default:
			fprintf(stderr, "Usage: %s -o out.mbtiles [-n name] [-l layername] [-z maxzoom] [-Z minzoom] [-d detail] [-D lower-detail] [-x excluded-field ...] [-y included-field ...] [-X] [-r droprate] [-b buffer] [-t tmpdir] [file.json]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (full_detail <= 0) {
		// ~0.5m accuracy at whatever zoom
		// 12 bits (4096 units) at z14

		full_detail = 26 - maxzoom;
	}

	if (outdir == NULL) {
		fprintf(stderr, "%s: must specify -o out.mbtiles\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (force) {
		unlink(outdir);
	}

	sqlite3 *outdb = mbtiles_open(outdir, argv);
	
	if (argc == optind + 1) {
		int i;
		for (i = optind; i < argc; i++) {
			FILE *f = fopen(argv[i], "r");
			if (f == NULL) {
				fprintf(stderr, "%s: %s: %s\n", argv[0], argv[i], strerror(errno));
			} else {
				read_json(f, name ? name : argv[i], layer, maxzoom, minzoom, outdb, &exclude, &include, exclude_all, droprate, buffer, tmpdir);
				fclose(f);
			}
		}
	} else if (argc > optind) {
		fprintf(stderr, "%s: Only accepts one input file\n", argv[0]);
		exit(EXIT_FAILURE);
	} else {
		read_json(stdin, name ? name : outdir, layer, maxzoom, minzoom, outdb, &exclude, &include, exclude_all, droprate, buffer, tmpdir);
	}

	mbtiles_close(outdb, argv);
	return 0;
}
