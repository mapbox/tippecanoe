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
#include <vector>
#include <string>
#include <set>
#include <map>
#include <cmath>

#ifdef __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/statfs.h>
#endif

extern "C" {
#include "jsonpull/jsonpull.h"
}

#include "mbtiles.hpp"
#include "tile.hpp"
#include "pool.hpp"
#include "projection.hpp"
#include "version.hpp"
#include "memfile.hpp"
#include "main.hpp"
#include "geojson.hpp"
#include "geometry.hpp"
#include "serial.hpp"
#include "options.hpp"

static int low_detail = 12;
static int full_detail = -1;
static int min_detail = 7;

int quiet = 0;
int geometry_scale = 0;
double simplification = 1;

int prevent[256];
int additional[256];

struct source {
	std::string layer;
	std::string file;
};

size_t CPUS;
size_t TEMP_FILES;
long long MAX_FILES;
static long long diskfree;

#define MAX_ZOOM 24

struct reader {
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

	long long file_bbox[4];

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

struct mergelist {
	long long start;
	long long end;

	struct mergelist *next;
};

static void insert(struct mergelist *m, struct mergelist **head, unsigned char *map) {
	while (*head != NULL && indexcmp(map + m->start, map + (*head)->start) > 0) {
		head = &((*head)->next);
	}

	m->next = *head;
	*head = m;
}

struct drop_state {
	double gap;
	unsigned long long previndex;
	double interval;
	double scale;
	double seq;
	long long included;
	unsigned x;
	unsigned y;
};

int calc_feature_minzoom(struct index *ix, struct drop_state *ds, int maxzoom, int basezoom, double droprate, double gamma) {
	int feature_minzoom = 0;
	unsigned xx, yy;
	decode(ix->index, &xx, &yy);

	if (gamma >= 0 && (ix->t == VT_POINT ||
			   (additional[A_LINE_DROP] && ix->t == VT_LINE) ||
			   (additional[A_POLYGON_DROP] && ix->t == VT_POLYGON))) {
		for (ssize_t i = maxzoom; i >= 0; i--) {
			// XXX This resets the feature counter at the start of each tile,
			// which makes the feature count come out close to what it is if
			// feature dropping happens during tiling. It means that the low
			// zooms are heavier than they legitimately should be though.
			{
				unsigned xxx = 0, yyy = 0;
				if (i != 0) {
					xxx = xx >> (32 - i);
					yyy = yy >> (32 - i);
				}
				if (ds[i].x != xxx || ds[i].y != yyy) {
					ds[i].seq = 0;
					ds[i].gap = 0;
					ds[i].previndex = 0;
				}
				ds[i].x = xxx;
				ds[i].y = yyy;
			}

			ds[i].seq++;
		}
		for (ssize_t i = maxzoom; i >= 0; i--) {
			if (ds[i].seq >= 0) {
				ds[i].seq -= ds[i].interval;
				ds[i].included++;
			} else {
				feature_minzoom = i + 1;
				break;
			}
		}

		// XXX manage_gap
	}

	return feature_minzoom;
}

static void merge(struct mergelist *merges, size_t nmerges, unsigned char *map, FILE *indexfile, int bytes, long long nrec, char *geom_map, FILE *geom_out, long long *geompos, long long *progress, long long *progress_max, long long *progress_reported, int maxzoom, int basezoom, double droprate, double gamma, struct drop_state *ds) {
	struct mergelist *head = NULL;

	for (size_t i = 0; i < nmerges; i++) {
		if (merges[i].start < merges[i].end) {
			insert(&(merges[i]), &head, map);
		}
	}

	while (head != NULL) {
		struct index ix = *((struct index *) (map + head->start));
		long long pos = *geompos;
		fwrite_check(geom_map + ix.start, 1, ix.end - ix.start, geom_out, "merge geometry");
		*geompos += ix.end - ix.start;
		int feature_minzoom = calc_feature_minzoom(&ix, ds, maxzoom, basezoom, droprate, gamma);
		serialize_byte(geom_out, feature_minzoom, geompos, "merge geometry");

		// Count this as an 75%-accomplishment, since we already 25%-counted it
		*progress += (ix.end - ix.start) * 3 / 4;
		if (!quiet && 100 * *progress / *progress_max != *progress_reported) {
			fprintf(stderr, "Reordering geometry: %lld%% \r", 100 * *progress / *progress_max);
			*progress_reported = 100 * *progress / *progress_max;
		}

		ix.start = pos;
		ix.end = *geompos;
		fwrite_check(&ix, bytes, 1, indexfile, "merge temporary");
		head->start += bytes;

		struct mergelist *m = head;
		head = m->next;
		m->next = NULL;

		if (m->start < m->end) {
			insert(m, &head, map);
		}
	}
}

struct sort_arg {
	int task;
	int cpus;
	long long indexpos;
	struct mergelist *merges;
	int indexfd;
	size_t nmerges;
	long long unit;
	int bytes;
};

void *run_sort(void *v) {
	struct sort_arg *a = (struct sort_arg *) v;

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

void do_read_parallel(char *map, long long len, long long initial_offset, const char *reading, struct reader *reader, volatile long long *progress_seq, std::set<std::string> *exclude, std::set<std::string> *include, int exclude_all, char *fname, int basezoom, int source, int nlayers, std::vector<std::map<std::string, layermap_entry> > *layermaps, double droprate, int *initialized, unsigned *initial_x, unsigned *initial_y, int maxzoom, std::string layername) {
	long long segs[CPUS + 1];
	segs[0] = 0;
	segs[CPUS] = len;

	for (size_t i = 1; i < CPUS; i++) {
		segs[i] = len * i / CPUS;

		while (segs[i] < len && map[segs[i]] != '\n') {
			segs[i]++;
		}
	}

	volatile long long layer_seq[CPUS];
	for (size_t i = 0; i < CPUS; i++) {
		// To preserve feature ordering, unique id for each segment
		// begins with that segment's offset into the input
		layer_seq[i] = segs[i] + initial_offset;
	}

	struct parse_json_args pja[CPUS];
	pthread_t pthreads[CPUS];
	std::vector<std::set<type_and_string> > file_subkeys;

	for (size_t i = 0; i < CPUS; i++) {
		file_subkeys.push_back(std::set<type_and_string>());
	}

	for (size_t i = 0; i < CPUS; i++) {
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
		pja[i].maxzoom = maxzoom;
		pja[i].layermap = &(*layermaps)[i];
		pja[i].layername = &layername;

		if (pthread_create(&pthreads[i], NULL, run_parse_json, &pja[i]) != 0) {
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
	}

	for (size_t i = 0; i < CPUS; i++) {
		void *retval;

		if (pthread_join(pthreads[i], &retval) != 0) {
			perror("pthread_join 370");
		}

		json_end_map(pja[i].jp);
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
	std::set<std::string> *exclude;
	std::set<std::string> *include;
	int exclude_all;
	char *fname;
	int maxzoom;
	int basezoom;
	int source;
	int nlayers;
	std::vector<std::map<std::string, layermap_entry> > *layermaps;
	double droprate;
	int *initialized;
	unsigned *initial_x;
	unsigned *initial_y;
	std::string layername;
};

void *run_read_parallel(void *v) {
	struct read_parallel_arg *rpa = (struct read_parallel_arg *) v;

	struct stat st;
	if (fstat(rpa->fd, &st) != 0) {
		perror("stat read temp");
	}
	if (rpa->len != st.st_size) {
		fprintf(stderr, "wrong number of bytes in temporary: %lld vs %lld\n", rpa->len, (long long) st.st_size);
	}
	rpa->len = st.st_size;

	char *map = (char *) mmap(NULL, rpa->len, PROT_READ, MAP_PRIVATE, rpa->fd, 0);
	if (map == NULL || map == MAP_FAILED) {
		perror("map intermediate input");
		exit(EXIT_FAILURE);
	}
	madvise(map, rpa->len, MADV_RANDOM);  // sequential, but from several pointers at once

	do_read_parallel(map, rpa->len, rpa->offset, rpa->reading, rpa->reader, rpa->progress_seq, rpa->exclude, rpa->include, rpa->exclude_all, rpa->fname, rpa->basezoom, rpa->source, rpa->nlayers, rpa->layermaps, rpa->droprate, rpa->initialized, rpa->initial_x, rpa->initial_y, rpa->maxzoom, rpa->layername);

	madvise(map, rpa->len, MADV_DONTNEED);
	if (munmap(map, rpa->len) != 0) {
		perror("munmap source file");
	}
	if (fclose(rpa->fp) != 0) {
		perror("close source file");
		exit(EXIT_FAILURE);
	}

	*(rpa->is_parsing) = 0;
	delete rpa;

	return NULL;
}

void start_parsing(int fd, FILE *fp, long long offset, long long len, volatile int *is_parsing, pthread_t *parallel_parser, bool &parser_created, const char *reading, struct reader *reader, volatile long long *progress_seq, std::set<std::string> *exclude, std::set<std::string> *include, int exclude_all, char *fname, int basezoom, int source, int nlayers, std::vector<std::map<std::string, layermap_entry> > &layermaps, double droprate, int *initialized, unsigned *initial_x, unsigned *initial_y, int maxzoom, std::string layername) {
	// This has to kick off an intermediate thread to start the parser threads,
	// so the main thread can get back to reading the next input stage while
	// the intermediate thread waits for the completion of the parser threads.

	*is_parsing = 1;

	struct read_parallel_arg *rpa = new struct read_parallel_arg;
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
	rpa->layermaps = &layermaps;
	rpa->droprate = droprate;
	rpa->initialized = initialized;
	rpa->initial_x = initial_x;
	rpa->initial_y = initial_y;
	rpa->maxzoom = maxzoom;
	rpa->layername = layername;

	if (pthread_create(parallel_parser, NULL, run_read_parallel, rpa) != 0) {
		perror("pthread_create");
		exit(EXIT_FAILURE);
	}
	parser_created = true;
}

void radix1(int *geomfds_in, int *indexfds_in, int inputs, int prefix, int splits, long long mem, const char *tmpdir, long long *availfiles, FILE *geomfile, FILE *indexfile, long long *geompos_out, long long *progress, long long *progress_max, long long *progress_reported, int maxzoom, int basezoom, double droprate, double gamma, struct drop_state *ds) {
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
			struct index *indexmap = (struct index *) mmap(NULL, indexst.st_size, PROT_READ, MAP_PRIVATE, indexfds_in[i], 0);
			if (indexmap == MAP_FAILED) {
				fprintf(stderr, "fd %lld, len %lld\n", (long long) indexfds_in[i], (long long) indexst.st_size);
				perror("map index");
				exit(EXIT_FAILURE);
			}
			madvise(indexmap, indexst.st_size, MADV_SEQUENTIAL);
			madvise(indexmap, indexst.st_size, MADV_WILLNEED);
			char *geommap = (char *) mmap(NULL, geomst.st_size, PROT_READ, MAP_PRIVATE, geomfds_in[i], 0);
			if (geommap == MAP_FAILED) {
				perror("map geom");
				exit(EXIT_FAILURE);
			}
			madvise(geommap, geomst.st_size, MADV_SEQUENTIAL);
			madvise(geommap, geomst.st_size, MADV_WILLNEED);

			for (size_t a = 0; a < indexst.st_size / sizeof(struct index); a++) {
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
			if (indexst.st_size + geomst.st_size < mem) {
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

				size_t nmerges = (indexpos + unit - 1) / unit;
				struct mergelist merges[nmerges];

				for (size_t a = 0; a < nmerges; a++) {
					merges[a].start = merges[a].end = 0;
				}

				pthread_t pthreads[CPUS];
				struct sort_arg args[CPUS];

				for (size_t a = 0; a < CPUS; a++) {
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

				for (size_t a = 0; a < CPUS; a++) {
					void *retval;

					if (pthread_join(pthreads[a], &retval) != 0) {
						perror("pthread_join 679");
					}
				}

				struct indexmap *indexmap = (struct indexmap *) mmap(NULL, indexst.st_size, PROT_READ, MAP_PRIVATE, indexfds[i], 0);
				if (indexmap == MAP_FAILED) {
					fprintf(stderr, "fd %lld, len %lld\n", (long long) indexfds[i], (long long) indexst.st_size);
					perror("map index");
					exit(EXIT_FAILURE);
				}
				madvise(indexmap, indexst.st_size, MADV_RANDOM);  // sequential, but from several pointers at once
				madvise(indexmap, indexst.st_size, MADV_WILLNEED);
				char *geommap = (char *) mmap(NULL, geomst.st_size, PROT_READ, MAP_PRIVATE, geomfds[i], 0);
				if (geommap == MAP_FAILED) {
					perror("map geom");
					exit(EXIT_FAILURE);
				}
				madvise(geommap, geomst.st_size, MADV_RANDOM);
				madvise(geommap, geomst.st_size, MADV_WILLNEED);

				merge(merges, nmerges, (unsigned char *) indexmap, indexfile, bytes, indexpos / bytes, geommap, geomfile, geompos_out, progress, progress_max, progress_reported, maxzoom, basezoom, droprate, gamma, ds);

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
				struct index *indexmap = (struct index *) mmap(NULL, indexst.st_size, PROT_READ, MAP_PRIVATE, indexfds[i], 0);
				if (indexmap == MAP_FAILED) {
					fprintf(stderr, "fd %lld, len %lld\n", (long long) indexfds[i], (long long) indexst.st_size);
					perror("map index");
					exit(EXIT_FAILURE);
				}
				madvise(indexmap, indexst.st_size, MADV_SEQUENTIAL);
				madvise(indexmap, indexst.st_size, MADV_WILLNEED);
				char *geommap = (char *) mmap(NULL, geomst.st_size, PROT_READ, MAP_PRIVATE, geomfds[i], 0);
				if (geommap == MAP_FAILED) {
					perror("map geom");
					exit(EXIT_FAILURE);
				}
				madvise(geommap, geomst.st_size, MADV_RANDOM);
				madvise(geommap, geomst.st_size, MADV_WILLNEED);

				for (size_t a = 0; a < indexst.st_size / sizeof(struct index); a++) {
					struct index ix = indexmap[a];
					long long pos = *geompos_out;

					fwrite_check(geommap + ix.start, ix.end - ix.start, 1, geomfile, "geom");
					*geompos_out += ix.end - ix.start;
					int feature_minzoom = calc_feature_minzoom(&ix, ds, maxzoom, basezoom, droprate, gamma);
					serialize_byte(geomfile, feature_minzoom, geompos_out, "merge geometry");

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

				radix1(&geomfds[i], &indexfds[i], 1, prefix + splitbits, *availfiles / 4, mem, tmpdir, availfiles, geomfile, indexfile, geompos_out, progress, progress_max, progress_reported, maxzoom, basezoom, droprate, gamma, ds);
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

void prep_drop_states(struct drop_state *ds, int maxzoom, int basezoom, double droprate) {
	// Needs to be signed for interval calculation
	for (ssize_t i = 0; i <= maxzoom; i++) {
		ds[i].gap = 0;
		ds[i].previndex = 0;
		ds[i].interval = 0;

		if (i < basezoom) {
			ds[i].interval = std::exp(std::log(droprate) * (basezoom - i));
		}

		ds[i].scale = (double) (1LL << (64 - 2 * (i + 8)));
		ds[i].seq = 0;
		ds[i].included = 0;
		ds[i].x = 0;
		ds[i].y = 0;
	}
}

void radix(struct reader *reader, int nreaders, FILE *geomfile, int geomfd, FILE *indexfile, int indexfd, const char *tmpdir, long long *geompos, int maxzoom, int basezoom, double droprate, double gamma) {
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
	for (int i = 0; i < nreaders; i++) {
		geomfds[i] = reader[i].geomfd;
		indexfds[i] = reader[i].indexfd;

		struct stat geomst;
		if (fstat(reader[i].geomfd, &geomst) < 0) {
			perror("stat geom");
			exit(EXIT_FAILURE);
		}
		geom_total += geomst.st_size;
	}

	struct drop_state ds[maxzoom + 1];
	prep_drop_states(ds, maxzoom, basezoom, droprate);

	long long progress = 0, progress_max = geom_total, progress_reported = -1;
	long long availfiles_before = availfiles;
	radix1(geomfds, indexfds, nreaders, 0, splits, mem, tmpdir, &availfiles, geomfile, indexfile, geompos, &progress, &progress_max, &progress_reported, maxzoom, basezoom, droprate, gamma, ds);

	if (availfiles - 2 * nreaders != availfiles_before) {
		fprintf(stderr, "Internal error: miscounted available file descriptors: %lld vs %lld\n", availfiles - 2 * nreaders, availfiles);
		exit(EXIT_FAILURE);
	}
}

int read_input(std::vector<source> &sources, char *fname, const char *layername, int maxzoom, int minzoom, int basezoom, double basezoom_marker_width, sqlite3 *outdb, std::set<std::string> *exclude, std::set<std::string> *include, int exclude_all, double droprate, int buffer, const char *tmpdir, double gamma, int read_parallel, int forcetable, const char *attribution) {
	int ret = EXIT_SUCCESS;

	struct reader reader[CPUS];
	for (size_t i = 0; i < CPUS; i++) {
		struct reader *r = reader + i;

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

		r->metafd = mkstemp(metaname);
		if (r->metafd < 0) {
			perror(metaname);
			exit(EXIT_FAILURE);
		}
		r->poolfd = mkstemp(poolname);
		if (r->poolfd < 0) {
			perror(poolname);
			exit(EXIT_FAILURE);
		}
		r->treefd = mkstemp(treename);
		if (r->treefd < 0) {
			perror(treename);
			exit(EXIT_FAILURE);
		}
		r->geomfd = mkstemp(geomname);
		if (r->geomfd < 0) {
			perror(geomname);
			exit(EXIT_FAILURE);
		}
		r->indexfd = mkstemp(indexname);
		if (r->indexfd < 0) {
			perror(indexname);
			exit(EXIT_FAILURE);
		}

		r->metafile = fopen(metaname, "wb");
		if (r->metafile == NULL) {
			perror(metaname);
			exit(EXIT_FAILURE);
		}
		r->poolfile = memfile_open(r->poolfd);
		if (r->poolfile == NULL) {
			perror(poolname);
			exit(EXIT_FAILURE);
		}
		r->treefile = memfile_open(r->treefd);
		if (r->treefile == NULL) {
			perror(treename);
			exit(EXIT_FAILURE);
		}
		r->geomfile = fopen(geomname, "wb");
		if (r->geomfile == NULL) {
			perror(geomname);
			exit(EXIT_FAILURE);
		}
		r->indexfile = fopen(indexname, "wb");
		if (r->indexfile == NULL) {
			perror(indexname);
			exit(EXIT_FAILURE);
		}
		r->metapos = 0;
		r->geompos = 0;
		r->indexpos = 0;

		unlink(metaname);
		unlink(poolname);
		unlink(treename);
		unlink(geomname);
		unlink(indexname);

		// To distinguish a null value
		{
			struct stringpool p;
			memfile_write(r->treefile, &p, sizeof(struct stringpool));
		}
		// Keep metadata file from being completely empty if no attributes
		serialize_int(r->metafile, 0, &r->metapos, "meta");

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
	for (size_t i = 0; i < CPUS; i++) {
		initialized[i] = initial_x[i] = initial_y[i] = 0;
	}

	size_t nlayers;
	if (layername != NULL) {
		nlayers = 1;
	} else {
		nlayers = sources.size();
		if (nlayers == 0) {
			nlayers = 1;
		}
	}

	std::vector<std::string> layernames;
	for (size_t l = 0; l < nlayers; l++) {
		if (layername != NULL) {
			layernames.push_back(std::string(layername));
		} else {
			const char *src;
			if (sources.size() < 1) {
				src = fname;
			} else if (sources[l].layer.size() != 0) {
				src = sources[l].layer.c_str();
			} else {
				src = sources[l].file.c_str();
			}

			// Find the last component of the pathname
			const char *ocp, *use = src;
			for (ocp = src; *ocp; ocp++) {
				if (*ocp == '/' && ocp[1] != '\0') {
					use = ocp + 1;
				}
			}
			std::string trunc = std::string(use);

			// Trim .json or .mbtiles from the name
			ssize_t cp;
			cp = trunc.find(".json");
			if (cp >= 0) {
				trunc = trunc.substr(0, cp);
			}
			cp = trunc.find(".mbtiles");
			if (cp >= 0) {
				trunc = trunc.substr(0, cp);
			}

			// Trim out characters that can't be part of selector
			std::string out;
			for (size_t p = 0; p < trunc.size(); p++) {
				if (isalpha(trunc[p]) || isdigit(trunc[p]) || trunc[p] == '_') {
					out.append(trunc, p, 1);
				}
			}
			layernames.push_back(out);

			if (!quiet) {
				fprintf(stderr, "For layer %d, using name \"%s\"\n", (int) l, out.c_str());
			}
		}
	}

	std::map<std::string, layermap_entry> layermap;
	for (size_t l = 0; l < nlayers; l++) {
		layermap.insert(std::pair<std::string, layermap_entry>(layernames[l], layermap_entry(l)));
	}
	std::vector<std::map<std::string, layermap_entry> > layermaps;
	for (size_t l = 0; l < CPUS; l++) {
		layermaps.push_back(layermap);
	}

	size_t nsources = sources.size();
	if (nsources == 0) {
		nsources = 1;
	}

	long overall_offset = 0;

	for (size_t source = 0; source < nsources; source++) {
		std::string reading;
		int fd;

		if (source >= sources.size()) {
			reading = "standard input";
			fd = 0;
		} else {
			reading = sources[source].file;
			fd = open(sources[source].file.c_str(), O_RDONLY);
			if (fd < 0) {
				perror(sources[source].file.c_str());
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
					map = (char *) mmap(NULL, st.st_size - off, PROT_READ, MAP_PRIVATE, fd, off);
					// No error if MAP_FAILED because check is below
					if (map != MAP_FAILED) {
						madvise(map, st.st_size - off, MADV_RANDOM);  // sequential, but from several pointers at once
					}
				}
			}
		}

		if (map != NULL && map != MAP_FAILED) {
			do_read_parallel(map, st.st_size - off, overall_offset, reading.c_str(), reader, &progress_seq, exclude, include, exclude_all, fname, basezoom, source, nlayers, &layermaps, droprate, initialized, initial_x, initial_y, maxzoom, layernames[source < nlayers ? source : 0]);
			overall_offset += st.st_size - off;
			checkdisk(reader, CPUS);

			if (munmap(map, st.st_size - off) != 0) {
				madvise(map, st.st_size, MADV_DONTNEED);
				perror("munmap source file");
			}
		} else {
			FILE *fp = fdopen(fd, "r");
			if (fp == NULL) {
				perror(sources[source].file.c_str());
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
				bool parser_created = false;

#define READ_BUF 2000
#define PARSE_MIN 10000000
#define PARSE_MAX (1LL * 1024 * 1024 * 1024)

				char buf[READ_BUF];
				int n;

				while ((n = fread(buf, sizeof(char), READ_BUF, fp)) > 0) {
					fwrite_check(buf, sizeof(char), n, readfp, reading.c_str());
					ahead += n;

					if (buf[n - 1] == '\n' && ahead > PARSE_MIN) {
						// Don't let the streaming reader get too far ahead of the parsers.
						// If the buffered input gets huge, even if the parsers are still running,
						// wait for the parser thread instead of continuing to stream input.

						if (is_parsing == 0 || ahead >= PARSE_MAX) {
							if (parser_created) {
								if (pthread_join(parallel_parser, NULL) != 0) {
									perror("pthread_join 1088");
									exit(EXIT_FAILURE);
								}
								parser_created = false;
							}

							fflush(readfp);
							start_parsing(readfd, readfp, initial_offset, ahead, &is_parsing, &parallel_parser, parser_created, reading.c_str(), reader, &progress_seq, exclude, include, exclude_all, fname, basezoom, source, nlayers, layermaps, droprate, initialized, initial_x, initial_y, maxzoom, layernames[source < nlayers ? source : 0]);

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
					perror(reading.c_str());
				}

				if (parser_created) {
					if (pthread_join(parallel_parser, NULL) != 0) {
						perror("pthread_join 1122");
						exit(EXIT_FAILURE);
					}
					parser_created = false;
				}

				fflush(readfp);

				if (ahead > 0) {
					start_parsing(readfd, readfp, initial_offset, ahead, &is_parsing, &parallel_parser, parser_created, reading.c_str(), reader, &progress_seq, exclude, include, exclude_all, fname, basezoom, source, nlayers, layermaps, droprate, initialized, initial_x, initial_y, maxzoom, layernames[source < nlayers ? source : 0]);

					if (parser_created) {
						if (pthread_join(parallel_parser, NULL) != 0) {
							perror("pthread_join 1133");
						}
						parser_created = false;
					}

					overall_offset += ahead;
					checkdisk(reader, CPUS);
				}
			} else {
				// Plain serial reading

				long long layer_seq = overall_offset;
				json_pull *jp = json_begin_file(fp);
				parse_json(jp, reading.c_str(), &layer_seq, &progress_seq, &reader[0].metapos, &reader[0].geompos, &reader[0].indexpos, exclude, include, exclude_all, reader[0].metafile, reader[0].geomfile, reader[0].indexfile, reader[0].poolfile, reader[0].treefile, fname, basezoom, source < nlayers ? source : 0, droprate, reader[0].file_bbox, 0, &initialized[0], &initial_x[0], &initial_y[0], reader, maxzoom, &layermaps[0], layernames[source < nlayers ? source : 0]);
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

	for (size_t i = 0; i < CPUS; i++) {
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

	for (size_t i = 0; i < CPUS; i++) {
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

	radix(reader, CPUS, geomfile, geomfd, indexfile, indexfd, tmpdir, &geompos, maxzoom, basezoom, droprate, gamma);

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
		fprintf(stderr, "%lld features, %lld bytes of geometry, %lld bytes of separate metadata, %lld bytes of string pool\n", progress_seq, geompos, metapos, poolpos);
	}

	if (indexpos == 0) {
		fprintf(stderr, "Did not read any valid geometries\n");
		exit(EXIT_FAILURE);
	}

	if (basezoom < 0 || droprate < 0) {
		struct index *map = (struct index *) mmap(NULL, indexpos, PROT_READ, MAP_PRIVATE, indexfd, 0);
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
			int z;
			for (z = 0; z <= MAX_ZOOM; z++) {
				tile[z].x = tile[z].y = tile[z].count = tile[z].fullcount = tile[z].gap = tile[z].previndex = 0;
				max[z].x = max[z].y = max[z].count = max[z].fullcount = 0;
			}
		}

		long long progress = -1;

		long long indices = indexpos / sizeof(struct index);
		long long ip;
		for (ip = 0; ip < indices; ip++) {
			unsigned xx, yy;
			decode(map[ip].index, &xx, &yy);

			long long nprogress = 100 * ip / indices;
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

				if (manage_gap(map[ip].index, &tile[z].previndex, scale, gamma, &tile[z].gap)) {
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

		// Fix up the minzooms for features, now that we really know the base zoom
		// and drop rate.

		struct stat geomst;
		if (fstat(geomfd, &geomst) != 0) {
			perror("stat sorted geom\n");
			exit(EXIT_FAILURE);
		}
		char *geom = (char *) mmap(NULL, geomst.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, geomfd, 0);
		if (geom == MAP_FAILED) {
			perror("mmap geom for fixup");
			exit(EXIT_FAILURE);
		}
		madvise(geom, indexpos, MADV_SEQUENTIAL);
		madvise(geom, indexpos, MADV_WILLNEED);

		struct drop_state ds[maxzoom + 1];
		prep_drop_states(ds, maxzoom, basezoom, droprate);

		for (ip = 0; ip < indices; ip++) {
			if (ip > 0 && map[ip].start != map[ip - 1].end) {
				fprintf(stderr, "Mismatched index at %lld: %lld vs %lld\n", ip, map[ip].start, map[ip].end);
			}
			int feature_minzoom = calc_feature_minzoom(&map[ip], ds, maxzoom, basezoom, droprate, gamma);
			geom[map[ip].end - 1] = feature_minzoom;
		}

		munmap(geom, geomst.st_size);
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

	for (size_t j = 1; j < TEMP_FILES; j++) {
		fd[j] = -1;
		size[j] = 0;
	}

	unsigned midx = 0, midy = 0;
	int written = traverse_zooms(fd, size, meta, stringpool, &midx, &midy, maxzoom, minzoom, basezoom, outdb, droprate, buffer, fname, tmpdir, gamma, full_detail, low_detail, min_detail, meta_off, pool_off, initial_x, initial_y, simplification, layermaps);

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
		madvise((void *) stringpool, poolpos, MADV_DONTNEED);
		if (munmap(stringpool, poolpos) != 0) {
			perror("munmap stringpool");
		}
	}
	if (close(poolfd) < 0) {
		perror("close pool");
	}

	double minlat = 0, minlon = 0, maxlat = 0, maxlon = 0, midlat = 0, midlon = 0;

	tile2lonlat(midx, midy, maxzoom, &minlon, &maxlat);
	tile2lonlat(midx + 1, midy + 1, maxzoom, &maxlon, &minlat);

	midlat = (maxlat + minlat) / 2;
	midlon = (maxlon + minlon) / 2;

	long long file_bbox[4] = {UINT_MAX, UINT_MAX, 0, 0};
	for (size_t i = 0; i < CPUS; i++) {
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

	tile2lonlat(file_bbox[0], file_bbox[1], 32, &minlon, &maxlat);
	tile2lonlat(file_bbox[2], file_bbox[3], 32, &maxlon, &minlat);

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

	std::map<std::string, layermap_entry> merged_lm = merge_layermaps(layermaps);
	if (additional[A_CALCULATE_FEATURE_DENSITY]) {
		for (auto ai = merged_lm.begin(); ai != merged_lm.end(); ++ai) {
			type_and_string tas;
			tas.type = VT_NUMBER;
			tas.string = "tippecanoe_feature_density";
			ai->second.file_keys.insert(tas);
		}
	}

	for (auto ai = merged_lm.begin(); ai != merged_lm.end(); ++ai) {
		ai->second.minzoom = minzoom;
		ai->second.maxzoom = maxzoom;
	}
	mbtiles_write_metadata(outdb, fname, minzoom, maxzoom, minlat, minlon, maxlat, maxlon, midlat, midlon, forcetable, attribution, merged_lm);

	return ret;
}

static bool has_name(struct option *long_options, int *pl) {
	for (size_t lo = 0; long_options[lo].name != NULL; lo++) {
		if (long_options[lo].flag == pl) {
			return true;
		}
	}

	return false;
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
	std::vector<source> sources;

	std::set<std::string> exclude, include;
	int exclude_all = 0;
	int read_parallel = 0;
	int files_open_at_start;

	for (i = 0; i < 256; i++) {
		prevent[i] = 0;
		additional[i] = 0;
	}

	static struct option long_options[] = {
		{"output", required_argument, 0, 'o'},

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
		{"exclude", required_argument, 0, 'x'},
		{"include", required_argument, 0, 'y'},
		{"drop-rate", required_argument, 0, 'r'},
		{"buffer", required_argument, 0, 'b'},
		{"temporary-directory", required_argument, 0, 't'},
		{"gamma", required_argument, 0, 'g'},
		{"prevent", required_argument, 0, 'p'},
		{"additional", required_argument, 0, 'a'},
		{"projection", required_argument, 0, 's'},
		{"simplification", required_argument, 0, 'S'},

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
		{"check-polygons", no_argument, &additional[A_DEBUG_POLYGON], 1},
		{"drop-polygons", no_argument, &additional[A_POLYGON_DROP], 1},
		{"prefer-radix-sort", no_argument, &additional[A_PREFER_RADIX_SORT], 1},
		{"calculate-feature-density", no_argument, &additional[A_CALCULATE_FEATURE_DENSITY], 1},
		{"detect-shared-borders", no_argument, &additional[A_DETECT_SHARED_BORDERS], 1},

		{"no-line-simplification", no_argument, &prevent[P_SIMPLIFY], 1},
		{"simplify-only-low-zooms", no_argument, &prevent[P_SIMPLIFY_LOW], 1},
		{"no-feature-limit", no_argument, &prevent[P_FEATURE_LIMIT], 1},
		{"no-tile-size-limit", no_argument, &prevent[P_KILOBYTE_LIMIT], 1},
		{"force-feature-limit", no_argument, &prevent[P_DYNAMIC_DROP], 1},
		{"preserve-input-order", no_argument, &prevent[P_INPUT_ORDER], 1},
		{"no-polygon-splitting", no_argument, &prevent[P_POLYGON_SPLIT], 1},
		{"no-clipping", no_argument, &prevent[P_CLIPPING], 1},
		{"no-duplication", no_argument, &prevent[P_DUPLICATION], 1},

		{0, 0, 0, 0},
	};

	{
		for (size_t lo = 0; long_options[lo].name != NULL; lo++) {
			if (long_options[lo].flag != NULL) {
				if (*long_options[lo].flag != 0) {
					fprintf(stderr, "Internal error: reused %s\n", long_options[lo].name);
					exit(EXIT_FAILURE);
				}
				*long_options[lo].flag = 1;
			}
		}

		for (size_t lo = 0; long_options[lo].name != NULL; lo++) {
			if (long_options[lo].flag != NULL) {
				*long_options[lo].flag = 0;
			}
		}
	}

	while ((i = getopt_long(argc, argv, "n:l:z:Z:B:d:D:m:o:x:y:r:b:t:g:p:a:XfFqvPL:A:s:S:", long_options, NULL)) != -1) {
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
			struct source src;
			src.layer = std::string(optarg).substr(0, cp - optarg);
			src.file = std::string(cp + 1);
			sources.push_back(src);
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
			exclude.insert(std::string(optarg));
			break;

		case 'y':
			exclude_all = 1;
			include.insert(std::string(optarg));
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
				if (has_name(long_options, &prevent[*cp & 0xFF])) {
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
				if (has_name(long_options, &additional[*cp & 0xFF])) {
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

		case 's':
			set_projection_or_exit(optarg);
			break;

		case 'S':
			simplification = atof(optarg);
			if (simplification <= 0) {
				fprintf(stderr, "%s: --simplification must be > 0\n", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;

		default: {
			int width = 7 + strlen(argv[0]);
			fprintf(stderr, "Usage: %s", argv[0]);
			for (size_t lo = 0; long_options[lo].name != NULL; lo++) {
				if (width + strlen(long_options[lo].name) + 9 >= 80) {
					fprintf(stderr, "\n        ");
					width = 8;
				}
				width += strlen(long_options[lo].name) + 9;
				if (strcmp(long_options[lo].name, "output") == 0) {
					fprintf(stderr, " --%s=output.mbtiles", long_options[lo].name);
					width += 9;
				} else if (long_options[lo].has_arg) {
					fprintf(stderr, " [--%s=...]", long_options[lo].name);
				} else {
					fprintf(stderr, " [--%s]", long_options[lo].name);
				}
			}
			if (width + 16 >= 80) {
				fprintf(stderr, "\n        ");
				width = 8;
			}
			fprintf(stderr, " [file.json ...]\n");
		}
			exit(EXIT_FAILURE);
		}
	}

	files_open_at_start = open("/dev/null", O_RDONLY);
	close(files_open_at_start);

	if (full_detail <= 0) {
		full_detail = 12;
	}

	if (full_detail < min_detail || low_detail < min_detail) {
		fprintf(stderr, "%s: Full detail and low detail must be at least minimum detail\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// Need two checks: one for geometry representation, the other for
	// index traversal when guessing base zoom and drop rate
	if (maxzoom > 32 - full_detail) {
		maxzoom = 32 - full_detail;
		fprintf(stderr, "Highest supported zoom with detail %d is %d\n", full_detail, maxzoom);
	}
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
		struct source src;
		src.layer = "";
		src.file = std::string(argv[i]);
		sources.push_back(src);
	}

	ret = read_input(sources, name ? name : outdir, layer, maxzoom, minzoom, basezoom, basezoom_marker_width, outdb, &exclude, &include, exclude_all, droprate, buffer, tmpdir, gamma, read_parallel, forcetable, attribution);

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
