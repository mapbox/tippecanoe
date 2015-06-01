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
#include "version.h"

int low_detail = 10;
int full_detail = -1;
int min_detail = 7;

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

size_t fwrite_check(const void *ptr, size_t size, size_t nitems, FILE *stream, const char *fname) {
	size_t w = fwrite(ptr, size, nitems, stream);
	if (w != nitems) {
		fprintf(stderr, "%s: Write to temporary file failed: %s\n", fname, strerror(errno));
		exit(EXIT_FAILURE);
	}
	return w;
}

void serialize_int(FILE *out, int n, long long *fpos, const char *fname) {
	fwrite_check(&n, sizeof(int), 1, out, fname);
	*fpos += sizeof(int);
}

void serialize_long_long(FILE *out, long long n, long long *fpos, const char *fname) {
	fwrite_check(&n, sizeof(long long), 1, out, fname);
	*fpos += sizeof(long long);
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

			serialize_byte(out, op, fpos, fname);
			serialize_uint(out, x, fpos, fname);
			serialize_uint(out, y, fpos, fname);
		} else {
			fprintf(stderr, "%s:%d: malformed point\n", fname, source->line);
		}
	}

	if (t == GEOM_POLYGON) {
		if (*fpos != began) {
			serialize_byte(out, VT_CLOSEPATH, fpos, fname);
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

int traverse_zooms(int geomfd[4], off_t geom_size[4], char *metabase, unsigned *file_bbox, struct pool **file_keys, unsigned *midx, unsigned *midy, char **layernames, int maxzoom, int minzoom, sqlite3 *outdb, double droprate, int buffer, const char *fname, const char *tmpdir, double gamma, int nlayers, char *prevent) {
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

				long long len = write_tile(&geom, metabase, file_bbox, z, x, y, z == maxzoom ? full_detail : low_detail, min_detail, maxzoom, file_keys, layernames, outdb, droprate, buffer, fname, sub, minzoom, maxzoom, todo, geomstart, along, gamma, nlayers, prevent);

				if (len < 0) {
					return i - 1;
				}

				if (z == maxzoom && len > most) {
					*midx = x;
					*midy = y;
					most = len;
				}
			}

			if (munmap(geomstart, geom_size[j]) != 0) {
				perror("munmap geom");
			}
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
	return maxzoom;
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

int read_json(int argc, char **argv, char *fname, const char *layername, int maxzoom, int minzoom, sqlite3 *outdb, struct pool *exclude, struct pool *include, int exclude_all, double droprate, int buffer, const char *tmpdir, double gamma, char *prevent) {
	int ret = EXIT_SUCCESS;

	char metaname[strlen(tmpdir) + strlen("/meta.XXXXXXXX") + 1];
	char geomname[strlen(tmpdir) + strlen("/geom.XXXXXXXX") + 1];
	char indexname[strlen(tmpdir) + strlen("/index.XXXXXXXX") + 1];

	sprintf(metaname, "%s%s", tmpdir, "/meta.XXXXXXXX");
	sprintf(geomname, "%s%s", tmpdir, "/geom.XXXXXXXX");
	sprintf(indexname, "%s%s", tmpdir, "/index.XXXXXXXX");

	int metafd = mkstemp(metaname);
	if (metafd < 0) {
		perror(metaname);
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
	unlink(geomname);
	unlink(indexname);

	unsigned file_bbox[] = { UINT_MAX, UINT_MAX, 0, 0 };
	unsigned midx = 0, midy = 0;
	long long seq = 0;

	int nlayers = argc;
	if (nlayers == 0) {
		nlayers = 1;
	}

	int n;
	for (n = 0; n < nlayers; n++) {
		json_pull *jp;
		const char *reading;
		FILE *fp;
		long long found_hashes = 0;
		long long found_features = 0;

		if (n >= argc) {
			reading = "standard input";
			fp = stdin;
		} else {
			reading = argv[n];
			fp = fopen(argv[n], "r");
			if (fp == NULL) {
				perror(argv[n]);
				continue;
			}
		}

		jp = json_begin_file(fp);

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

				if (found_hashes == 50 && found_features == 0) {
					fprintf(stderr, "%s:%d: Not finding any GeoJSON features in input. Is your file just bare geometries?\n", reading, jp->line);
					break;
				}
			}

			json_object *type = json_hash_get(j, "type");
			if (type == NULL || type->type != JSON_STRING || strcmp(type->string, "Feature") != 0) {
				continue;
			}

			found_features++;

			json_object *geometry = json_hash_get(j, "geometry");
			if (geometry == NULL) {
				fprintf(stderr, "%s:%d: feature with no geometry\n", reading, jp->line);
				json_free(j);
				continue;
			}

			json_object *geometry_type = json_hash_get(geometry, "type");
			if (geometry_type == NULL) {
				static int warned = 0;
				if (!warned) {
					fprintf(stderr, "%s:%d: null geometry (additional not reported)\n", reading, jp->line);
					warned = 1;
				}

				json_free(j);
				continue;
			}

			if (geometry_type->type != JSON_STRING) {
				fprintf(stderr, "%s:%d: geometry without type\n", reading, jp->line);
				json_free(j);
				continue;
			}

			json_object *properties = json_hash_get(j, "properties");
			if (properties == NULL || (properties->type != JSON_HASH && properties->type != JSON_NULL)) {
				fprintf(stderr, "%s:%d: feature without properties hash\n", reading, jp->line);
				json_free(j);
				continue;
			}

			json_object *coordinates = json_hash_get(geometry, "coordinates");
			if (coordinates == NULL || coordinates->type != JSON_ARRAY) {
				fprintf(stderr, "%s:%d: feature without coordinates array\n", reading, jp->line);
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
				fprintf(stderr, "%s:%d: Can't handle geometry type %s\n", reading, jp->line, geometry_type->string);
				json_free(j);
				continue;
			}

			{
				unsigned bbox[] = { UINT_MAX, UINT_MAX, 0, 0 };

				int nprop = 0;
				if (properties->type == JSON_HASH) {
					nprop = properties->length;
				}

				long long metastart = metapos;
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
							json_free(j);
							continue;
						}
					}
				}

				serialize_int(metafile, m, &metapos, fname);
				for (i = 0; i < m; i++) {
					serialize_int(metafile, metatype[i], &metapos, fname);
					serialize_string(metafile, metakey[i], &metapos, fname);
					serialize_string(metafile, metaval[i], &metapos, fname);
				}

				long long geomstart = geompos;

				serialize_byte(geomfile, mb_geometry[t], &geompos, fname);
				serialize_byte(geomfile, n, &geompos, fname);
				serialize_long_long(geomfile, metastart, &geompos, fname);
				parse_geometry(t, coordinates, bbox, &geompos, geomfile, VT_MOVETO, fname, jp);
				serialize_byte(geomfile, VT_END, &geompos, fname);

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

				serialize_byte(geomfile, minzoom, &geompos, fname);

				struct index index;
				index.start = geomstart;
				index.end = geompos;
				index.index = encode(bbox[0] / 2 + bbox[2] / 2, bbox[1] / 2 + bbox[3] / 2);
				fwrite_check(&index, sizeof(struct index), 1, indexfile, fname);
				indexpos += sizeof(struct index);

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

		json_end(jp);
		fclose(fp);
	}

	fclose(metafile);
	fclose(geomfile);
	fclose(indexfile);

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

	struct pool file_keys1[nlayers];
	struct pool *file_keys[nlayers];
	int i;
	for (i = 0; i < nlayers; i++) {
		pool_init(&file_keys1[i], 0);
		file_keys[i] = &file_keys1[i];
	}

	char *layernames[nlayers];
	for (i = 0; i < nlayers; i++) {
		if (argc <= 1 && layername != NULL) {
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
			layername = trunc;

			char *out = trunc;
			for (cp = trunc; *cp; cp++) {
				if (isalpha(*cp) || isdigit(*cp) || *cp == '_') {
					*out++ = *cp;
				}
			}
			*out = '\0';

			printf("using layer %d name %s\n", i, trunc);
		}
	}

	/* Sort the index by geometry */

	{
		int bytes = sizeof(struct index);
		fprintf(stderr, "Sorting %lld features\n", (long long) indexpos / bytes);

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

	/* Copy geometries to a new file in index order */

	indexfd = open(indexname, O_RDONLY);
	if (indexfd < 0) {
		perror("reopen sorted index");
		exit(EXIT_FAILURE);
	}
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
				fprintf(stderr, "Reordering geometry: %3.1f%%\r", p / 10.0);
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

	int written = traverse_zooms(fd, size, meta, file_bbox, file_keys, &midx, &midy, layernames, maxzoom, minzoom, outdb, droprate, buffer, fname, tmpdir, gamma, nlayers, prevent);

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

	mbtiles_write_metadata(outdb, fname, layernames, minzoom, maxzoom, minlat, minlon, maxlat, maxlon, midlat, midlon, file_keys, nlayers); // XXX layers

	for (i = 0; i < nlayers; i++) {
		pool_free_strings(&file_keys1[i]);
		free(layernames[i]);
	}
	return ret;
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
	double gamma = 0;
	int buffer = 5;
	const char *tmpdir = "/tmp";
	char prevent[256];

	struct pool exclude, include;
	pool_init(&exclude, 0);
	pool_init(&include, 0);
	int exclude_all = 0;

	for (i = 0; i < 256; i++) {
		prevent[i] = 0;
	}

	while ((i = getopt(argc, argv, "l:n:z:Z:d:D:m:o:x:y:r:b:fXt:g:p:v")) != -1) {
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

		case 'g':
			gamma = atof(optarg);
			break;

		case 'p':
			{
				char *cp;
				for (cp = optarg; *cp != '\0'; cp++) {
					prevent[*cp & 0xFF] = 1;
				}
			}
			break;

		case 'v':
			fprintf(stderr, VERSION);
			exit(EXIT_FAILURE);

		default:
			fprintf(stderr, "Usage: %s -o out.mbtiles [-n name] [-l layername] [-z maxzoom] [-Z minzoom] [-d detail] [-D lower-detail] [-m min-detail] [-x excluded-field ...] [-y included-field ...] [-X] [-r droprate] [-b buffer] [-t tmpdir] [-p rcfs] [file.json ...]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (minzoom > maxzoom) {
		fprintf(stderr, "minimum zoom -Z cannot be greater than maxzoom -z\n");
		exit(EXIT_FAILURE);
	}

	if (full_detail <= 0) {
		// ~0.5m accuracy at whatever zoom
		// 12 bits (4096 units) at z14

		full_detail = 26 - maxzoom;
	}

	if (full_detail < min_detail || low_detail < min_detail) {
		fprintf(stderr, "%s: Full detail and low detail must be at least minimum detail\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (outdir == NULL) {
		fprintf(stderr, "%s: must specify -o out.mbtiles\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (force) {
		unlink(outdir);
	}

	sqlite3 *outdb = mbtiles_open(outdir, argv);
	int ret = EXIT_SUCCESS;

	ret = read_json(argc - optind, argv + optind, name ? name : outdir, layer, maxzoom, minzoom, outdb, &exclude, &include, exclude_all, droprate, buffer, tmpdir, gamma, prevent);

	mbtiles_close(outdb, argv);
	return ret;
}
