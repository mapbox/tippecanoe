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
#include <jsonpull.h>
#include "util.h"

int mapbits = 2 * (16 + 8); // zoom level 16
int metabits = 0;
int version = 2;

#define MAX_INPUT 2000

struct file {
	int legs;
	int level;
	FILE *f;

	struct file *next;
};

struct pool {
	char *s;
	long long off;
	struct pool *left;
	struct pool *right;
};

long long poolString(char *s, struct pool **pool, FILE *extra, long long *xoff) {
	while (1) {
		if (*pool == NULL) {
			*pool = malloc(sizeof(struct pool));
			(*pool)->s = strdup(s);
			(*pool)->off = *xoff;
			*xoff += fwrite(s, sizeof(char), strlen(s) + 1, extra);
			(*pool)->left = NULL;
			(*pool)->right = NULL;

			return (*pool)->off;
		}

		int cmp = strcmp(s, (*pool)->s);

		if (cmp == 0) {
			return (*pool)->off;
		} else if (cmp < 0) {
			pool = &((*pool)->left);
		} else {
			pool = &((*pool)->right);
		}
	}
}

void usage(char *name) {
	fprintf(stderr, "Usage: %s [-z zoom] [-m metadata-bits] -o destdir [file ...]\n",
		name);
}

char *dequote(char **cp, int *type) {
	char *start = *cp;
	char *out = *cp;

	if (**cp == '"') {
		*type = META_STRING;
	} else {
		*type = META_INTEGER;
	}

	int within = 0;
	for (; **cp; (*cp)++) {
		if (**cp == '"') {
			within = !within;
		} else if (**cp == '\\' && (cp[0][1] == '\\' || cp[0][1] == '\"')) {
			*out++ = cp[0][1];
			(*cp)++;
		} else if (**cp == '\\' && cp[0][1] == 'u') {
			char hex[5] = "aaaa";
			memcpy(hex, &(cp[0][2]), 4);
			*cp += 5;

			unsigned long ch = strtoul(hex, NULL, 16);
			if (ch <= 0x7F) {
				*out++ = ch;
			} else if (ch <= 0x7FF) {
				*out++ = 0xC0 | (ch >> 6);
				*out++ = 0x80 | (ch & 0x3F);
			} else {
				*out++ = 0xE0 | (ch >> 12);
				*out++ = 0x80 | ((ch >> 6) & 0x3F);
				*out++ = 0x80 | (ch & 0x3F);
			}
		} else if (isspace(**cp) && !within) {
			break;
		} else {
			*out++ = cp[0][0];
		}
	}

	if (*cp) {
		(*cp)++;
	}

	*out = '\0';
	return start;
}

void encode(char *destdir, struct file **files, int *maxn, FILE *extra, long long *xoff, struct pool **pool,
	    int n, double *lat, double *lon,
	    int m, int *metasize, char **metaname, char **meta, int *metatype);

void read_json(FILE *f, char *destdir, struct file **files, int *maxn, FILE *extra, long long *xoff, struct pool **pool) {
	json_pull *jp = json_begin_file(f);

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
		if (type != NULL && type->type == JSON_STRING && strcmp(type->string, "Feature") == 0) {
			json_object *geometry = json_hash_get(j, "geometry");

			if (geometry != NULL) {
				json_object *geometry_type = json_hash_get(geometry, "type");
				if (geometry_type != NULL && geometry_type->type == JSON_STRING) {
					int t = -1;

					if (strcmp(geometry_type->string, "Point") == 0) {
						t = GEOM_POINT;
					} else if (strcmp(geometry_type->string, "LineString") == 0) {
						t = GEOM_LINESTRING;
					} else {
						fprintf(stderr, "%d: Can't handle geometry type %s\n", jp->line, geometry_type->string);
					}

					if (t != -1) {
						json_object *properties = json_hash_get(j, "properties");
						json_object *coordinates = json_hash_get(geometry, "coordinates");

						if (properties != NULL && properties->type == JSON_HASH) {
							int metasize[properties->length];
							char *metaname[properties->length];
							char *meta[properties->length];
							int metatype[properties->length];
							int m = 0;

							int i;
							for (i = 0; i < properties->length; i++) {
								if (properties->keys[i]->type == JSON_STRING) {
									metasize[m] = metabits;
									metaname[m] = properties->keys[i]->string;

									if (properties->values[i] != NULL && properties->values[i]->type == JSON_STRING) {
										metatype[m] = META_STRING;
										meta[m] = properties->values[i]->string;
										m++;
									} else if (properties->values[i] != NULL && properties->values[i]->type == JSON_NUMBER) {
										metatype[m] = META_INTEGER;
										meta[m] = properties->values[i]->string;
										m++;
									} else {
										fprintf(stderr, "%d: Unsupported meta type\n", jp->line);
									}
								}
							}

							if (coordinates != NULL && coordinates->type == JSON_ARRAY) {
								int n;

								if (t == GEOM_POINT) {
									n = 1;
								} else {
									n = coordinates->length;
								}

								double lon[n], lat[n];
								int ok = 1;

								if (t == GEOM_POINT) {
									if (coordinates->length >= 2) {
										lon[0] = coordinates->array[0]->number;
										lat[0] = coordinates->array[1]->number;
									} else {
										fprintf(stderr, "%d: not enough coordinates for a point\n", jp->line);
										ok = 0;
									}
								} else {
									for (i = 0; i < n; i++) {
										if (coordinates->array[i]->type == JSON_ARRAY &&
										    coordinates->array[i]->length >= 2) {
											lon[i] = coordinates->array[i]->array[0]->number;
											lat[i] = coordinates->array[i]->array[1]->number;
										} else {
											fprintf(stderr, "%d: not enough coordinates within the geometry\n", jp->line);
											ok = 0;
										}
									}
								}

								if (ok) {
									encode(destdir, files, maxn, extra, xoff, pool, n, lat, lon, m, metasize, metaname, meta, metatype);
								}
							} else {
								fprintf(stderr, "%d: feature geometry with no coordinates\n", jp->line);
							}
						} else {
							fprintf(stderr, "%d: feature with no properties\n", jp->line);
						}
					}
				}
			} else {
				fprintf(stderr, "%d: feature with no geometry\n", jp->line);
			}

			json_free(j);
		}
	}

	json_end(jp);
}

void read_file(FILE *f, char *destdir, struct file **files, int *maxn, FILE *extra, long long *xoff, struct pool **pool) {
	char s[MAX_INPUT];
	double lat[MAX_INPUT], lon[MAX_INPUT];
	int metasize[MAX_INPUT];
	char *meta[MAX_INPUT];
	char *metaname[MAX_INPUT];
	int metatype[MAX_INPUT];
	unsigned long long seq = 0;
	long long maxmeta = -1;

	if (version == 2) {
		metabits = 40;
	}

	int c = getc(f);
	if (c != EOF) {
		ungetc(c, f);
	}

	if (c == '{' || c == '[') {
		read_json(f, destdir, files, maxn, extra, xoff, pool);
		return;
	}

	while (fgets(s, MAX_INPUT, f)) {
		char *cp = s;
		int n = 0, m = 0;

		if (seq % 100000 == 0) {
			fprintf(stderr, "Read %.1f million records\r", seq / 1000000.0);
		}
		seq++;

		while (1) {
			if (sscanf(cp, "%lf,%lf", &lat[n], &lon[n]) == 2) {
				n++;
				while (*cp != '\0' && *cp != ' ') {
					cp++;
				}
				while (*cp == ' ') {
					cp++;
				}
			} else if (sscanf(cp, "%d:", &metasize[m]) == 1) {
				metasize[m] = atoi(cp);
				while (*cp && isdigit(*cp)) {
					cp++;
				}
				if (*cp == ':') {
					cp++;
				}

				meta[m] = dequote(&cp, &metatype[m]);

				if (atoll(meta[m]) > maxmeta && atoll(meta[m]) >= (1LLU << metabits)) {
					fprintf(stderr, "Warning: metadata %s too big for -m%d\n", meta[m], metabits);
					maxmeta = atoll(meta[m]);
				}
				metaname[m] = "meta";
				m++;
				while (*cp == ' ') {
					cp++;
				}
			} else if (*cp == ':') {
				metasize[m] = metabits;
				cp++;

				meta[m] = dequote(&cp, &metatype[m]);

				if (atoll(meta[m]) > maxmeta && atoll(meta[m]) >= (1LLU << metabits)) {
					fprintf(stderr, "Warning: metadata %s too big for -m%d\n", meta[m], metabits);
					maxmeta = atoll(meta[m]);
				}
				metaname[m] = "meta";
				m++;
				while (*cp == ' ') {
					cp++;
				}
			} else if (*cp == '/' && cp[1] == '/') {
				break;
			} else if (*cp == '=' || isalpha(*cp)) {
				metasize[m] = metabits;

				if (*cp == '=') {
					cp++;
				}

				metaname[m] = cp;
				for (; *cp; cp++) {
					if (*cp == '=') {
						break;
					}
				}

				if (*cp == 0) {
					fprintf(stderr, "Didn't find value for %s\n", metaname[m]);
					break;
				}

				*cp = '\0';
				cp++;

				meta[m] = dequote(&cp, &metatype[m]);
				m++;
				while (*cp == ' ') {
					cp++;
				}
			} else {
				break;
			}
		}

		if (n == 0) {
			fprintf(stderr, "No valid points in %s", s);
			continue;
		}

		encode(destdir, files, maxn, extra, xoff, pool, n, lat, lon, m, metasize, metaname, meta, metatype);
	}
}

void encode(char *destdir, struct file **files, int *maxn, FILE *extra, long long *xoff, struct pool **pool,
	    int n, double *lat, double *lon,
	    int m, int *metasize, char **metaname, char **meta, int *metatype) {
	unsigned int x[MAX_INPUT], y[MAX_INPUT];

	{
		// Project each point to web mercator

		int i;
		for (i = 0; i < n; i++) {
			if (lat[i] > 85.0511 || lat[i] < -85.0511) {
				fprintf(stderr, "Can't represent latitude %f\n", lat[i]);
				n = 0;
				break;
			}
			if (lon[i] >= 180 || lon[i] <= -180) {
				fprintf(stderr, "Can't represent longitude %f\n", lon[i]);
				n = 0;
				break;
			}

			latlon2tile(lat[i], lon[i], 32, &x[i], &y[i]);
		}

		if (n == 0) {
			return;
		}

		// If this is a polyline, find out how many leading bits in common
		// all the points have.

		int common = 0;
		int components = n;

		if (n > 1) {
			int ok = 1;
			for (common = 0; ok && common < mapbits / 2; common++) {
				int x0 = x[0] & (1 << (31 - common));
				int y0 = y[0] & (1 << (31 - common));

				for (i = 1; i < n; i++) {
					if ((x[i] & (1 << (31 - common))) != x0 ||
					    (y[i] & (1 << (31 - common))) != y0) {
						ok = 0;
						break;
					}
				}

				if (!ok) {
					break;
				}
			}

			if (version >= 2) {
				components = n;
				n = 1;
				*maxn = 1;
			} else {
				if (n > *maxn) {
					*maxn = n;
				}
			}
		} else {
			if (n > *maxn) {
				*maxn = n;
			}
		}

		int bytes = bytesfor(mapbits, metabits, n, common);
		unsigned char buf[bytes];
		memset(buf, 0, bytes);

		int off = 0;
		xy2buf(x[0], y[0], buf, &off, mapbits, 0);
		for (i = 1; i < n; i++) {
			xy2buf(x[i], y[i], buf, &off, mapbits, common);
		}

		if (version >= 2) {
			long long keys[m], values[m];

			for (i = 0; i < m; i++) {
				keys[i] = poolString(metaname[i], pool, extra, xoff);

				if (metatype[i] == META_STRING) {
					values[i] = poolString(meta[i], pool, extra, xoff);
				}
			}

			meta2buf(metabits, *xoff, buf, &off, bytes * 8);
			long long here = *xoff;

			if (components > 1) {
				n = 0;
			}

			if (components == 1) {
				*xoff += writeSigned(extra, (components << GEOM_TYPE_BITS) | GEOM_POINT);
			} else { // XXX additional types
				*xoff += writeSigned(extra, (components << GEOM_TYPE_BITS) | GEOM_LINESTRING);
			}

			int s = 32 - (mapbits / 2);
			for (i = 1; i < components; i++) {
				*xoff += writeSigned(extra, (long long) (x[i] >> s) - (long long) (x[i - 1] >> s));
				*xoff += writeSigned(extra, (long long) (y[i] >> s) - (long long) (y[i - 1] >> s));
			}

			*xoff += writeSigned(extra, m);
			for (i = 0; i < m; i++) {
				*xoff += writeSigned(extra, keys[i] - here);
				*xoff += writeSigned(extra, metatype[i]);

				if (metatype[i] == META_STRING) {
					// string
					*xoff += writeSigned(extra, values[i] - here);
				} else if (metatype[i] == META_INTEGER) {
					// integer 
					*xoff += writeSigned(extra, atoll(meta[i]));
				} else {
					// XXX
					fprintf(stderr, "unsupported type %d\n", metatype[i]);
					exit(EXIT_FAILURE);
				}
			}
		} else {
			for (i = 0; i < m; i++) {
				meta2buf(metasize[i], atoll(meta[i]), buf, &off, bytes * 8);
			}
		}

		struct file **fo;

		for (fo = files; *fo != NULL; fo = &((*fo)->next)) {
			if ((*fo)->legs == n && (*fo)->level == common) {
				break;
			}
		}

		if (*fo == NULL) {
			*fo = malloc(sizeof(struct file));

			if (*fo == NULL) {
				perror("malloc");
				exit(EXIT_FAILURE);
			}

			char fn[strlen(destdir) + 10 + 1 + 10 + 1];
			sprintf(fn, "%s/%d,%d", destdir, n, common);

			(*fo)->next = NULL;
			(*fo)->legs = n;
			(*fo)->level = common;
			(*fo)->f = fopen(fn, "w");

			if ((*fo)->f == NULL) {
				perror(fn);
				exit(EXIT_FAILURE);
			}
		}

		fwrite(buf, sizeof(char), bytes, (*fo)->f);
	}
}

struct merge {
	long long start;
	long long end;

	struct merge *next;
};

void insert(struct merge *m, struct merge **head, unsigned char *map, int bytes) {
	while (*head != NULL && memcmp(map + m->start, map + (*head)->start, bytes) > 0) {
		head = &((*head)->next);
	}

	m->next = *head;
	*head = m;
}

void merge(struct merge *merges, int nmerges, unsigned char *map, FILE *f, int bytes, long long nrec) {
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

int main(int argc, char **argv) {
	int i;
	extern int optind;
	extern char *optarg;
	char *destdir = NULL;
	struct pool *pool = NULL;

	while ((i = getopt(argc, argv, "z:m:o:")) != -1) {
		switch (i) {
		case 'z':
			mapbits = 2 * (atoi(optarg) + 8);
			break;

		case 'm':
			metabits = atoi(optarg);
			break;

		case 'o':
			destdir = optarg;
			break;

		default:
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (mapbits <= 8) {
		fprintf(stderr, "%s: Zoom level (-z) must be > 0\n", argv[0]);
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (mapbits % 8 != 0) {
		int b = (mapbits + 7) & ~7;
		fprintf(stderr, "%s: Using zoom level %d, not %d\n", argv[0],
			b / 2 - 8, mapbits / 2 - 8);
		mapbits = b;
	}

	if (destdir == NULL) {
		fprintf(stderr, "%s: Must specify a directory with -o\n", argv[0]);
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (mkdir(destdir, 0777) != 0) {
		perror(destdir);
		exit(EXIT_FAILURE);
	}

	char s[strlen(destdir) + 6 + 1];
	sprintf(s, "%s/extra", destdir);
	FILE *extra = NULL;
	long long xoff = 0;
	if (version >= 2) {
		extra = fopen(s, "wb");
		if (extra == NULL) {
			perror(s);
			exit(EXIT_FAILURE);
		}
		putc('\0', extra);
		xoff++;
	}

	struct file *files = NULL;
	int maxn = 0;

	if (optind == argc) {
		read_file(stdin, destdir, &files, &maxn, extra, &xoff, &pool);
	} else {
		for (i = optind; i < argc; i++) {
			FILE *f = fopen(argv[i], "r");
			if (f == NULL) {
				perror(argv[i]);
				exit(EXIT_FAILURE);
			}

			read_file(f, destdir, &files, &maxn, extra, &xoff, &pool);
			fclose(f);
		}
	}

	sprintf(s, "%s/meta", destdir);
	FILE *f = fopen(s, "w");
	if (f == NULL) {
		perror(s);
		exit(EXIT_FAILURE);
	}
	fprintf(f, "%d\n", version);
	fprintf(f, "%d %d %d\n", mapbits, metabits, maxn);
	fclose(f);

	if (version >= 2) {
		off_t fp = ftello(extra);

		if (fp != xoff) {
			fprintf(stderr, "Thought file position was %lld but it was %lld\n",
				(long long) xoff, (long long) fp);
		}

		fclose(extra);
	}

	for (; files != NULL; files = files->next) {
		fclose(files->f);

		char fn[strlen(destdir) + 10 + 1 + 10 + 1];
		sprintf(fn, "%s/%d,%d", destdir, files->legs, files->level);

		int fd = open(fn, O_RDWR);
		if (fd < 0) {
			perror(fn);
			exit(EXIT_FAILURE);
		}

		struct stat st;
		if (fstat(fd, &st) < 0) {
			perror("stat");
			exit(EXIT_FAILURE);
		}

		int bytes;
		if (files->legs == 0) {
			bytes = bytesfor(mapbits, metabits, 1, files->level);
		} else {
			bytes = bytesfor(mapbits, metabits, files->legs, files->level);
		}
		gSortBytes = bytes;

		fprintf(stderr,
		 	"Sorting %lld shapes sized for zoom level %d\n",
			(long long) st.st_size / bytes,
			files->level);

		int page = sysconf(_SC_PAGESIZE);
		long long unit = (50 * 1024 * 1024 / bytes) * bytes;
		while (unit % page != 0) {
			unit += bytes;
		}

		int nmerges = (st.st_size + unit - 1) / unit;
		struct merge merges[nmerges];

		long long start;
		for (start = 0; start < st.st_size; start += unit) {
			long long end = start + unit;
			if (end > st.st_size) {
				end = st.st_size;
			}

			if (nmerges != 1) {
				fprintf(stderr, "Sorting part %lld of %d\r", start / unit + 1, nmerges);
			}

			merges[start / unit].start = start;
			merges[start / unit].end = end;
			merges[start / unit].next = NULL;

			void *map = mmap(NULL, end - start, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, start);
			if (map == MAP_FAILED) {
				perror("mmap");
				exit(EXIT_FAILURE);
			}

			qsort(map, (end - start) / bytes, bytes, bufcmp);

			// Sorting and then copying avoids the need to
			// write out intermediate stages of the sort.

			void *map2 = mmap(NULL, end - start, PROT_READ | PROT_WRITE, MAP_SHARED, fd, start);
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

		void *map = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
		if (map == MAP_FAILED) {
			perror("mmap");
			exit(EXIT_FAILURE);
		}

		if (unlink(fn) != 0) {
			perror("unlink");
			exit(EXIT_FAILURE);
		}

		FILE *f = fopen(fn, "w");
		if (f == NULL) {
			perror(fn);
			exit(EXIT_FAILURE);
		}

		merge(merges, nmerges, map, f, bytes, st.st_size / bytes);

		munmap(map, st.st_size);
		fclose(f);
		close(fd);
	}

	fprintf(stderr, "\n");

	return 0;
}
