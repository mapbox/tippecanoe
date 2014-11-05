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

int indexcmp(const void *v1, const void *v2) {
	const struct index *i1 = v1;
	const struct index *i2 = v2;

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

size_t fwrite_check(const void *ptr, size_t size, size_t nitems, FILE *stream, char *fname, json_pull *source) {
	size_t w = fwrite(ptr, size, nitems, stream);
	if (w != nitems) {
		fprintf(stderr, "%s:%d: Write to temporary file failed: %s\n", fname, source->line, strerror(errno));
		exit(EXIT_FAILURE);
	}
	return w;
}

void serialize_int(FILE *out, int n, long long *fpos, char *fname, json_pull *source) {
	fwrite_check(&n, sizeof(int), 1, out, fname, source);
	*fpos += sizeof(int);
}

void serialize_uint(FILE *out, unsigned n, long long *fpos, char *fname, json_pull *source) {
	fwrite_check(&n, sizeof(unsigned), 1, out, fname, source);
	*fpos += sizeof(unsigned);
}

void serialize_string(FILE *out, char *s, long long *fpos, char *fname, json_pull *source) {
	int len = strlen(s);

	serialize_int(out, len + 1, fpos, fname, source);
	fwrite_check(s, sizeof(char), len, out, fname, source);
	fwrite_check("", sizeof(char), 1, out, fname, source);
	*fpos += len + 1;
}

void parse_geometry(int t, json_object *j, unsigned *bbox, long long *fpos, FILE *out, int op, char *fname, json_pull *source) {
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

			serialize_int(out, op, fpos, fname, source);
			serialize_uint(out, x, fpos, fname, source);
			serialize_uint(out, y, fpos, fname, source);
		} else {
			fprintf(stderr, "%s:%d: malformed point", fname, source->line);
		}
	}

	if (t == GEOM_POLYGON) {
		if (*fpos != began) {
			serialize_int(out, VT_CLOSEPATH, fpos, fname, source);
		}
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

void check(struct index *ix, long long n, char *metabase, unsigned *file_bbox, struct pool *file_keys, unsigned *midx, unsigned *midy, char *layername, int maxzoom, int minzoom, sqlite3 *outdb, double droprate, int buffer) {
	fprintf(stderr, "\n");
	long long most = 0;

	int z;
	for (z = maxzoom; z >= minzoom; z--) {
		struct index *i, *j = NULL;
		for (i = ix; i < ix + n && i != NULL; i = j) {
			if (i > ix && indexcmp(i - 1, i) > 0) {
				fprintf(stderr, "index out of order\n");
				exit(EXIT_FAILURE);
			}
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

			long long len = write_tile(i, j, metabase, file_bbox, z, tx, ty, z == maxzoom ? full_detail : low_detail, maxzoom, file_keys, layername, outdb, droprate, buffer);

			if (z == maxzoom && len > most) {
				*midx = tx;
				*midy = ty;
				most = len;
			}
		}
	}

	fprintf(stderr, "\n");
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

void read_json(FILE *f, char *fname, char *layername, int maxzoom, int minzoom, sqlite3 *outdb, struct pool *exclude, int exclude_all, double droprate, int buffer, char *tmpdir) {
	char metaname[strlen(tmpdir) + strlen("/meta.XXXXXXXX") + 1];
	char indexname[strlen(tmpdir) + strlen("/index.XXXXXXXX") + 1];

	sprintf(metaname, "%s%s", tmpdir, "/meta.XXXXXXXX");
	sprintf(indexname, "%s%s", tmpdir, "/index.XXXXXXXX");

	int metafd = mkstemp(metaname);
	if (metafd < 0) {
		perror(metaname);
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
	FILE *indexfile = fopen(indexname, "wb");
	if (indexfile == NULL) {
		perror(indexname);
		exit(EXIT_FAILURE);
	}
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
			goto next_feature;
		}

		json_object *geometry_type = json_hash_get(geometry, "type");
		if (geometry_type == NULL || geometry_type->type != JSON_STRING) {
			fprintf(stderr, "%s:%d: geometry without type string\n", fname, jp->line);
			goto next_feature;
		}

		json_object *properties = json_hash_get(j, "properties");
		if (properties == NULL || properties->type != JSON_HASH) {
			fprintf(stderr, "%s:%d: feature without properties hash\n", fname, jp->line);
			goto next_feature;
		}

		json_object *coordinates = json_hash_get(geometry, "coordinates"); 
		if (coordinates == NULL || coordinates->type != JSON_ARRAY) {
			fprintf(stderr, "%s:%d: feature without coordinates array\n", fname, jp->line);
			goto next_feature;
		}

		int t;
		for (t = 0; t < GEOM_TYPES; t++) {
			if (strcmp(geometry_type->string, geometry_names[t]) == 0) {
				break;
			}
		}
		if (t >= GEOM_TYPES) {
			fprintf(stderr, "%s:%d: Can't handle geometry type %s\n", fname, jp->line, geometry_type->string);
			goto next_feature;
		}

		{
			long long start = fpos;

			unsigned bbox[] = { UINT_MAX, UINT_MAX, 0, 0 };

			parse_geometry(t, coordinates, bbox, &fpos, metafile, VT_MOVETO, fname, jp);
			serialize_int(metafile, VT_END, &fpos, fname, jp);

			char *metakey[properties->length];
			char *metaval[properties->length];
			int metatype[properties->length];
			int m = 0;

			int i;
			for (i = 0; i < properties->length; i++) {
				if (properties->keys[i]->type == JSON_STRING) {
					if (exclude_all || is_pooled(exclude, properties->keys[i]->string, VT_STRING)) {
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
					} else {
						fprintf(stderr, "%s:%d: Unsupported property type for %s\n", fname, jp->line, properties->keys[i]->string);
						goto next_feature;
					}
				}
			}

			serialize_int(metafile, m, &fpos, fname, jp);
			for (i = 0; i < m; i++) {
				serialize_int(metafile, metatype[i], &fpos, fname, jp);
				serialize_string(metafile, metakey[i], &fpos, fname, jp);
				serialize_string(metafile, metaval[i], &fpos, fname, jp);
			}

			int z = maxzoom;

			unsigned cx = bbox[0] / 2 + bbox[2] / 2;
			unsigned cy = bbox[1] / 2 + bbox[3] / 2;

			/* XXX do proper overlap instead of whole bounding box */
			if (z == 0) {
				struct index ix;
				ix.index = encode(cx, cy);
				ix.fpos = start;
				fwrite_check(&ix, sizeof(struct index), 1, indexfile, fname, jp);
			} else {
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

							struct index ix;

							if (x == cx >> (32 - z) && y == cy >> (32 - z)) {
								ix.index = encode(cx, cy);
							} else {
								ix.index = encode(x << (32 - z), y << (32 - z));
							}
							ix.fpos = start;
							ix.type = mb_geometry[t];
							ix.maxzoom = z;
							fwrite_check(&ix, sizeof(struct index), 1, indexfile, fname, jp);
						}
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
			
			if (seq % 10000 == 0) {
				fprintf(stderr, "Read %.2f million features\r", seq / 1000000.0);
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

	struct stat metast;
	fstat(metafd, &metast);
	char *meta = mmap(NULL, metast.st_size, PROT_READ, MAP_PRIVATE, metafd, 0);
	if (meta == MAP_FAILED) {
		perror("mmap meta");
		exit(EXIT_FAILURE);
	}

	struct pool file_keys;
	pool_init(&file_keys, 0);

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

		merge(merges, nmerges, map, f, bytes, indexst.st_size / bytes);

		munmap(map, indexst.st_size);
		fclose(f);
		close(indexfd);
	}

	indexfd = open(indexname, O_RDONLY);
	if (indexfd < 0) {
		perror(indexname);
		exit(EXIT_FAILURE);
	}
	if (unlink(indexname) != 0) {
		perror(indexname);
		exit(EXIT_FAILURE);
	}

	struct index *index = mmap(NULL, indexst.st_size, PROT_READ, MAP_PRIVATE, indexfd, 0);
	if (index == MAP_FAILED) {
		perror("mmap index");
		exit(EXIT_FAILURE);
	}

	check(index, indexst.st_size / sizeof(struct index), meta, file_bbox, &file_keys, &midx, &midy, layername, maxzoom, minzoom, outdb, droprate, buffer);

	munmap(index, indexst.st_size);
	munmap(meta, metast.st_size);

	close(indexfd);
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
	char *tmpdir = "/tmp";

	struct pool exclude;
	pool_init(&exclude, 0);
	int exclude_all = 0;

	while ((i = getopt(argc, argv, "l:n:z:Z:d:D:o:x:r:b:fXt:")) != -1) {
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
			fprintf(stderr, "Usage: %s -o out.mbtiles [-n name] [-l layername] [-z maxzoom] [-Z minzoom] [-d detail] [-D lower-detail] [-x excluded-field ...] [-X] [-r droprate] [-b buffer] [-t tmpdir] [file.json]\n", argv[0]);
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
				read_json(f, name ? name : argv[i], layer, maxzoom, minzoom, outdb, &exclude, exclude_all, droprate, buffer, tmpdir);
				fclose(f);
			}
		}
	} else if (argc > optind) {
		fprintf(stderr, "%s: Only accepts one input file\n", argv[0]);
		exit(EXIT_FAILURE);
	} else {
		read_json(stdin, name ? name : outdir, layer, maxzoom, minzoom, outdb, &exclude, exclude_all, droprate, buffer, tmpdir);
	}

	mbtiles_close(outdb, argv);
	return 0;
}
