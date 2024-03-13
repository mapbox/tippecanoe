#ifdef MTRACE
#include <mcheck.h>
#endif

#ifdef __APPLE__
#define _DARWIN_UNLIMITED_STREAMS
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
#include <signal.h>
#include <sys/time.h>
#include <zlib.h>
#include <algorithm>
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

#include "jsonpull/jsonpull.h"
#include "mbtiles.hpp"
#include "tile.hpp"
#include "pool.hpp"
#include "projection.hpp"
#include "version.hpp"
#include "memfile.hpp"
#include "main.hpp"
#include "geojson.hpp"
#include "geobuf.hpp"
#include "geocsv.hpp"
#include "geometry.hpp"
#include "serial.hpp"
#include "options.hpp"
#include "mvt.hpp"
#include "dirtiles.hpp"
#include "evaluator.hpp"
#include "text.hpp"

static int low_detail = 12;
static int full_detail = -1;
static int min_detail = 7;

int quiet = 0;
int quiet_progress = 0;
double progress_interval = 0;
std::atomic<double> last_progress(0);
int geometry_scale = 0;
double simplification = 1;
size_t max_tile_size = 500000;
size_t max_tile_features = 200000;
int cluster_distance = 0;
long justx = -1, justy = -1;
std::string attribute_for_id = "";

int prevent[256];
int additional[256];

struct source {
	std::string layer = "";
	std::string file = "";
	std::string description = "";
	std::string format = "";
};

size_t CPUS;
size_t TEMP_FILES;
long long MAX_FILES;
static long long diskfree;
char **av;

std::vector<clipbbox> clipbboxes;

void checkdisk(std::vector<struct reader> *r) {
	long long used = 0;
	for (size_t i = 0; i < r->size(); i++) {
		// Meta, pool, and tree are used once.
		// Geometry and index will be duplicated during sorting and tiling.
		used += (*r)[i].metapos + 2 * (*r)[i].geompos + 2 * (*r)[i].indexpos + (*r)[i].poolfile->len + (*r)[i].treefile->len;
	}

	static int warned = 0;
	if (used > diskfree * .9 && !warned) {
		fprintf(stderr, "You will probably run out of disk space.\n%lld bytes used or committed, of %lld originally available\n", used, diskfree);
		warned = 1;
	}
};

int atoi_require(const char *s, const char *what) {
	char *err = NULL;
	if (*s == '\0') {
		fprintf(stderr, "%s: %s must be a number (got %s)\n", *av, what, s);
		exit(EXIT_FAILURE);
	}
	int ret = strtol(s, &err, 10);
	if (*err != '\0') {
		fprintf(stderr, "%s: %s must be a number (got %s)\n", *av, what, s);
		exit(EXIT_FAILURE);
	}
	return ret;
}

double atof_require(const char *s, const char *what) {
	char *err = NULL;
	if (*s == '\0') {
		fprintf(stderr, "%s: %s must be a number (got %s)\n", *av, what, s);
		exit(EXIT_FAILURE);
	}
	double ret = strtod(s, &err);
	if (*err != '\0') {
		fprintf(stderr, "%s: %s must be a number (got %s)\n", *av, what, s);
		exit(EXIT_FAILURE);
	}
	return ret;
}

long long atoll_require(const char *s, const char *what) {
	char *err = NULL;
	if (*s == '\0') {
		fprintf(stderr, "%s: %s must be a number (got %s)\n", *av, what, s);
		exit(EXIT_FAILURE);
	}
	long long ret = strtoll(s, &err, 10);
	if (*err != '\0') {
		fprintf(stderr, "%s: %s must be a number (got %s)\n", *av, what, s);
		exit(EXIT_FAILURE);
	}
	return ret;
}

void init_cpus() {
	const char *TIPPECANOE_MAX_THREADS = getenv("TIPPECANOE_MAX_THREADS");

	if (TIPPECANOE_MAX_THREADS != NULL) {
		CPUS = atoi_require(TIPPECANOE_MAX_THREADS, "TIPPECANOE_MAX_THREADS");
	} else {
		CPUS = sysconf(_SC_NPROCESSORS_ONLN);
	}

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
		fds[i] = open("/dev/null", O_RDONLY | O_CLOEXEC);
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

	if (i1->ix < i2->ix) {
		return -1;
	} else if (i1->ix > i2->ix) {
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

int calc_feature_minzoom(struct index *ix, struct drop_state *ds, int maxzoom, double gamma) {
	int feature_minzoom = 0;
	unsigned xx, yy;
	decode_index(ix->ix, &xx, &yy);

	if (gamma >= 0 && (ix->t == VT_POINT ||
			   (additional[A_LINE_DROP] && ix->t == VT_LINE) ||
			   (additional[A_POLYGON_DROP] && ix->t == VT_POLYGON))) {
		for (ssize_t i = maxzoom; i >= 0; i--) {
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

static void merge(struct mergelist *merges, size_t nmerges, unsigned char *map, FILE *indexfile, int bytes, char *geom_map, FILE *geom_out, std::atomic<long long> *geompos, long long *progress, long long *progress_max, long long *progress_reported, int maxzoom, double gamma, struct drop_state *ds) {
	struct mergelist *head = NULL;

	for (size_t i = 0; i < nmerges; i++) {
		if (merges[i].start < merges[i].end) {
			insert(&(merges[i]), &head, map);
		}
	}

	last_progress = 0;

	while (head != NULL) {
		struct index ix = *((struct index *) (map + head->start));
		long long pos = *geompos;
		fwrite_check(geom_map + ix.start, 1, ix.end - ix.start, geom_out, "merge geometry");
		*geompos += ix.end - ix.start;
		int feature_minzoom = calc_feature_minzoom(&ix, ds, maxzoom, gamma);
		serialize_byte(geom_out, feature_minzoom, geompos, "merge geometry");

		// Count this as an 75%-accomplishment, since we already 25%-counted it
		*progress += (ix.end - ix.start) * 3 / 4;
		if (!quiet && !quiet_progress && progress_time() && 100 * *progress / *progress_max != *progress_reported) {
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

	sort_arg(int task1, int cpus1, long long indexpos1, struct mergelist *merges1, int indexfd1, size_t nmerges1, long long unit1, int bytes1)
	    : task(task1), cpus(cpus1), indexpos(indexpos1), merges(merges1), indexfd(indexfd1), nmerges(nmerges1), unit(unit1), bytes(bytes1) {
	}
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

void do_read_parallel(char *map, long long len, long long initial_offset, const char *reading, std::vector<struct reader> *readers, std::atomic<long long> *progress_seq, std::set<std::string> *exclude, std::set<std::string> *include, int exclude_all, int basezoom, int source, std::vector<std::map<std::string, layermap_entry> > *layermaps, int *initialized, unsigned *initial_x, unsigned *initial_y, int maxzoom, std::string layername, bool uses_gamma, std::map<std::string, int> const *attribute_types, int separator, double *dist_sum, size_t *dist_count, bool want_dist, bool filters) {
	long long segs[CPUS + 1];
	segs[0] = 0;
	segs[CPUS] = len;

	for (size_t i = 1; i < CPUS; i++) {
		segs[i] = len * i / CPUS;

		while (segs[i] < len && map[segs[i]] != separator) {
			segs[i]++;
		}
	}

	double dist_sums[CPUS];
	size_t dist_counts[CPUS];

	std::atomic<long long> layer_seq[CPUS];
	for (size_t i = 0; i < CPUS; i++) {
		// To preserve feature ordering, unique id for each segment
		// begins with that segment's offset into the input
		layer_seq[i] = segs[i] + initial_offset;
		dist_sums[i] = dist_counts[i] = 0;
	}

	std::vector<parse_json_args> pja;

	std::vector<serialization_state> sst;
	sst.resize(CPUS);

	pthread_t pthreads[CPUS];
	std::vector<std::set<type_and_string> > file_subkeys;

	for (size_t i = 0; i < CPUS; i++) {
		file_subkeys.push_back(std::set<type_and_string>());
	}

	for (size_t i = 0; i < CPUS; i++) {
		sst[i].fname = reading;
		sst[i].line = 0;
		sst[i].layer_seq = &layer_seq[i];
		sst[i].progress_seq = progress_seq;
		sst[i].readers = readers;
		sst[i].segment = i;
		sst[i].initialized = &initialized[i];
		sst[i].initial_x = &initial_x[i];
		sst[i].initial_y = &initial_y[i];
		sst[i].dist_sum = &(dist_sums[i]);
		sst[i].dist_count = &(dist_counts[i]);
		sst[i].want_dist = want_dist;
		sst[i].maxzoom = maxzoom;
		sst[i].uses_gamma = uses_gamma;
		sst[i].filters = filters;
		sst[i].layermap = &(*layermaps)[i];
		sst[i].exclude = exclude;
		sst[i].include = include;
		sst[i].exclude_all = exclude_all;
		sst[i].basezoom = basezoom;
		sst[i].attribute_types = attribute_types;

		pja.push_back(parse_json_args(
			json_begin_map(map + segs[i], segs[i + 1] - segs[i]),
			source,
			&layername,
			&sst[i]));
	}

	for (size_t i = 0; i < CPUS; i++) {
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

		*dist_sum += dist_sums[i];
		*dist_count += dist_counts[i];

		json_end_map(pja[i].jp);
	}
}

static ssize_t read_stream(json_pull *j, char *buffer, size_t n);

struct STREAM {
	FILE *fp = NULL;
	gzFile gz = NULL;

	int fclose() {
		int ret;

		if (gz != NULL) {
			ret = gzclose(gz);
		} else {
			ret = ::fclose(fp);
		}

		delete this;
		return ret;
	}

	int peekc() {
		if (gz != NULL) {
			int c = gzgetc(gz);
			if (c != EOF) {
				gzungetc(c, gz);
			}
			return c;
		} else {
			int c = getc(fp);
			if (c != EOF) {
				ungetc(c, fp);
			}
			return c;
		}
	}

	size_t read(char *out, size_t count) {
		if (gz != NULL) {
			int ret = gzread(gz, out, count);
			if (ret < 0) {
				fprintf(stderr, "%s: Error reading compressed data\n", *av);
				exit(EXIT_FAILURE);
			}
			return ret;
		} else {
			return ::fread(out, 1, count, fp);
		}
	}

	json_pull *json_begin() {
		return ::json_begin(read_stream, this);
	}
};

static ssize_t read_stream(json_pull *j, char *buffer, size_t n) {
	return ((STREAM *) j->source)->read(buffer, n);
}

STREAM *streamfdopen(int fd, const char *mode, std::string const &fname) {
	STREAM *s = new STREAM;
	s->fp = NULL;
	s->gz = NULL;

	if (fname.size() > 3 && fname.substr(fname.size() - 3) == std::string(".gz")) {
		s->gz = gzdopen(fd, mode);
		if (s->gz == NULL) {
			fprintf(stderr, "%s: %s: Decompression error\n", *av, fname.c_str());
			exit(EXIT_FAILURE);
		}
	} else {
		s->fp = fdopen(fd, mode);
		if (s->fp == NULL) {
			perror(fname.c_str());
			exit(EXIT_FAILURE);
		}
	}

	return s;
}

STREAM *streamfpopen(FILE *fp) {
	STREAM *s = new STREAM;
	s->fp = fp;
	s->gz = NULL;

	return s;
}

struct read_parallel_arg {
	int fd = 0;
	STREAM *fp = NULL;
	long long offset = 0;
	long long len = 0;
	std::atomic<int> *is_parsing = NULL;
	int separator = 0;

	const char *reading = NULL;
	std::vector<struct reader> *readers = NULL;
	std::atomic<long long> *progress_seq = NULL;
	std::set<std::string> *exclude = NULL;
	std::set<std::string> *include = NULL;
	int exclude_all = 0;
	int maxzoom = 0;
	int basezoom = 0;
	int source = 0;
	std::vector<std::map<std::string, layermap_entry> > *layermaps = NULL;
	int *initialized = NULL;
	unsigned *initial_x = NULL;
	unsigned *initial_y = NULL;
	std::string layername = "";
	bool uses_gamma = false;
	std::map<std::string, int> const *attribute_types = NULL;
	double *dist_sum = NULL;
	size_t *dist_count = NULL;
	bool want_dist = false;
	bool filters = false;
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

	do_read_parallel(map, rpa->len, rpa->offset, rpa->reading, rpa->readers, rpa->progress_seq, rpa->exclude, rpa->include, rpa->exclude_all, rpa->basezoom, rpa->source, rpa->layermaps, rpa->initialized, rpa->initial_x, rpa->initial_y, rpa->maxzoom, rpa->layername, rpa->uses_gamma, rpa->attribute_types, rpa->separator, rpa->dist_sum, rpa->dist_count, rpa->want_dist, rpa->filters);

	madvise(map, rpa->len, MADV_DONTNEED);
	if (munmap(map, rpa->len) != 0) {
		perror("munmap source file");
	}
	if (rpa->fp->fclose() != 0) {
		perror("close source file");
		exit(EXIT_FAILURE);
	}

	*(rpa->is_parsing) = 0;
	delete rpa;

	return NULL;
}

void start_parsing(int fd, STREAM *fp, long long offset, long long len, std::atomic<int> *is_parsing, pthread_t *parallel_parser, bool &parser_created, const char *reading, std::vector<struct reader> *readers, std::atomic<long long> *progress_seq, std::set<std::string> *exclude, std::set<std::string> *include, int exclude_all, int basezoom, int source, std::vector<std::map<std::string, layermap_entry> > &layermaps, int *initialized, unsigned *initial_x, unsigned *initial_y, int maxzoom, std::string layername, bool uses_gamma, std::map<std::string, int> const *attribute_types, int separator, double *dist_sum, size_t *dist_count, bool want_dist, bool filters) {
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
	rpa->separator = separator;

	rpa->reading = reading;
	rpa->readers = readers;
	rpa->progress_seq = progress_seq;
	rpa->exclude = exclude;
	rpa->include = include;
	rpa->exclude_all = exclude_all;
	rpa->basezoom = basezoom;
	rpa->source = source;
	rpa->layermaps = &layermaps;
	rpa->initialized = initialized;
	rpa->initial_x = initial_x;
	rpa->initial_y = initial_y;
	rpa->maxzoom = maxzoom;
	rpa->layername = layername;
	rpa->uses_gamma = uses_gamma;
	rpa->attribute_types = attribute_types;
	rpa->dist_sum = dist_sum;
	rpa->dist_count = dist_count;
	rpa->want_dist = want_dist;
	rpa->filters = filters;

	if (pthread_create(parallel_parser, NULL, run_read_parallel, rpa) != 0) {
		perror("pthread_create");
		exit(EXIT_FAILURE);
	}
	parser_created = true;
}

void radix1(int *geomfds_in, int *indexfds_in, int inputs, int prefix, int splits, long long mem, const char *tmpdir, long long *availfiles, FILE *geomfile, FILE *indexfile, std::atomic<long long> *geompos_out, long long *progress, long long *progress_max, long long *progress_reported, int maxzoom, int basezoom, double droprate, double gamma, struct drop_state *ds) {
	// Arranged as bits to facilitate subdividing again if a subdivided file is still huge
	int splitbits = log(splits) / log(2);
	splits = 1 << splitbits;

	FILE *geomfiles[splits];
	FILE *indexfiles[splits];
	int geomfds[splits];
	int indexfds[splits];
	std::atomic<long long> sub_geompos[splits];

	int i;
	for (i = 0; i < splits; i++) {
		sub_geompos[i] = 0;

		char geomname[strlen(tmpdir) + strlen("/geom.XXXXXXXX") + 1];
		sprintf(geomname, "%s%s", tmpdir, "/geom.XXXXXXXX");
		char indexname[strlen(tmpdir) + strlen("/index.XXXXXXXX") + 1];
		sprintf(indexname, "%s%s", tmpdir, "/index.XXXXXXXX");

		geomfds[i] = mkstemp_cloexec(geomname);
		if (geomfds[i] < 0) {
			perror(geomname);
			exit(EXIT_FAILURE);
		}
		indexfds[i] = mkstemp_cloexec(indexname);
		if (indexfds[i] < 0) {
			perror(indexname);
			exit(EXIT_FAILURE);
		}

		geomfiles[i] = fopen_oflag(geomname, "wb", O_WRONLY | O_CLOEXEC);
		if (geomfiles[i] == NULL) {
			perror(geomname);
			exit(EXIT_FAILURE);
		}
		indexfiles[i] = fopen_oflag(indexname, "wb", O_WRONLY | O_CLOEXEC);
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
				unsigned long long which = (ix.ix << prefix) >> (64 - splitbits);
				long long pos = sub_geompos[which];

				fwrite_check(geommap + ix.start, ix.end - ix.start, 1, geomfiles[which], "geom");
				sub_geompos[which] += ix.end - ix.start;

				// Count this as a 25%-accomplishment, since we will copy again
				*progress += (ix.end - ix.start) / 4;
				if (!quiet && !quiet_progress && progress_time() && 100 * *progress / *progress_max != *progress_reported) {
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
				std::atomic<long long> indexpos(indexst.st_size);
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
				if (unit < page) {
					unit = page;
				}

				size_t nmerges = (indexpos + unit - 1) / unit;
				struct mergelist merges[nmerges];

				for (size_t a = 0; a < nmerges; a++) {
					merges[a].start = merges[a].end = 0;
				}

				pthread_t pthreads[CPUS];
				std::vector<sort_arg> args;

				for (size_t a = 0; a < CPUS; a++) {
					args.push_back(sort_arg(
						a,
						CPUS,
						indexpos,
						merges,
						indexfds[i],
						nmerges,
						unit,
						bytes));
				}

				for (size_t a = 0; a < CPUS; a++) {
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

				merge(merges, nmerges, (unsigned char *) indexmap, indexfile, bytes, geommap, geomfile, geompos_out, progress, progress_max, progress_reported, maxzoom, gamma, ds);

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
					int feature_minzoom = calc_feature_minzoom(&ix, ds, maxzoom, gamma);
					serialize_byte(geomfile, feature_minzoom, geompos_out, "merge geometry");

					// Count this as an 75%-accomplishment, since we already 25%-counted it
					*progress += (ix.end - ix.start) * 3 / 4;
					if (!quiet && !quiet_progress && progress_time() && 100 * *progress / *progress_max != *progress_reported) {
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

void radix(std::vector<struct reader> &readers, int nreaders, FILE *geomfile, FILE *indexfile, const char *tmpdir, std::atomic<long long> *geompos, int maxzoom, int basezoom, double droprate, double gamma) {
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
		geomfds[i] = readers[i].geomfd;
		indexfds[i] = readers[i].indexfd;

		struct stat geomst;
		if (fstat(readers[i].geomfd, &geomst) < 0) {
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

void choose_first_zoom(long long *file_bbox, std::vector<struct reader> &readers, unsigned *iz, unsigned *ix, unsigned *iy, int minzoom, int buffer) {
	for (size_t i = 0; i < CPUS; i++) {
		if (readers[i].file_bbox[0] < file_bbox[0]) {
			file_bbox[0] = readers[i].file_bbox[0];
		}
		if (readers[i].file_bbox[1] < file_bbox[1]) {
			file_bbox[1] = readers[i].file_bbox[1];
		}
		if (readers[i].file_bbox[2] > file_bbox[2]) {
			file_bbox[2] = readers[i].file_bbox[2];
		}
		if (readers[i].file_bbox[3] > file_bbox[3]) {
			file_bbox[3] = readers[i].file_bbox[3];
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

	for (ssize_t z = minzoom; z >= 0; z--) {
		long long shift = 1LL << (32 - z);

		long long left = (file_bbox[0] - buffer * shift / 256) / shift;
		long long top = (file_bbox[1] - buffer * shift / 256) / shift;
		long long right = (file_bbox[2] + buffer * shift / 256) / shift;
		long long bottom = (file_bbox[3] + buffer * shift / 256) / shift;

		if (left == right && top == bottom) {
			*iz = z;
			*ix = left;
			*iy = top;
			break;
		}
	}
}

int read_input(std::vector<source> &sources, char *fname, int maxzoom, int minzoom, int basezoom, double basezoom_marker_width, sqlite3 *outdb, const char *outdir, std::set<std::string> *exclude, std::set<std::string> *include, int exclude_all, json_object *filter, double droprate, int buffer, const char *tmpdir, double gamma, int read_parallel, int forcetable, const char *attribution, bool uses_gamma, long long *file_bbox, const char *prefilter, const char *postfilter, const char *description, bool guess_maxzoom, std::map<std::string, int> const *attribute_types, const char *pgm, std::map<std::string, attribute_op> const *attribute_accum, std::map<std::string, std::string> const &attribute_descriptions, std::string const &commandline) {
	int ret = EXIT_SUCCESS;

	std::vector<struct reader> readers;
	readers.resize(CPUS);
	for (size_t i = 0; i < CPUS; i++) {
		struct reader *r = &readers[i];

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

		r->metafd = mkstemp_cloexec(metaname);
		if (r->metafd < 0) {
			perror(metaname);
			exit(EXIT_FAILURE);
		}
		r->poolfd = mkstemp_cloexec(poolname);
		if (r->poolfd < 0) {
			perror(poolname);
			exit(EXIT_FAILURE);
		}
		r->treefd = mkstemp_cloexec(treename);
		if (r->treefd < 0) {
			perror(treename);
			exit(EXIT_FAILURE);
		}
		r->geomfd = mkstemp_cloexec(geomname);
		if (r->geomfd < 0) {
			perror(geomname);
			exit(EXIT_FAILURE);
		}
		r->indexfd = mkstemp_cloexec(indexname);
		if (r->indexfd < 0) {
			perror(indexname);
			exit(EXIT_FAILURE);
		}

		r->metafile = fopen_oflag(metaname, "wb", O_WRONLY | O_CLOEXEC);
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
		r->geomfile = fopen_oflag(geomname, "wb", O_WRONLY | O_CLOEXEC);
		if (r->geomfile == NULL) {
			perror(geomname);
			exit(EXIT_FAILURE);
		}
		r->indexfile = fopen_oflag(indexname, "wb", O_WRONLY | O_CLOEXEC);
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
	if (fstatfs(readers[0].geomfd, &fsstat) != 0) {
		perror("fstatfs");
		exit(EXIT_FAILURE);
	}
	diskfree = (long long) fsstat.f_bsize * fsstat.f_bavail;

	std::atomic<long long> progress_seq(0);

	// 2 * CPUS: One per reader thread, one per tiling thread
	int initialized[2 * CPUS];
	unsigned initial_x[2 * CPUS], initial_y[2 * CPUS];
	for (size_t i = 0; i < 2 * CPUS; i++) {
		initialized[i] = initial_x[i] = initial_y[i] = 0;
	}

	size_t nlayers = sources.size();
	for (size_t l = 0; l < nlayers; l++) {
		if (sources[l].layer.size() == 0) {
			const char *src;
			if (sources[l].file.size() == 0) {
				src = fname;
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

			std::vector<std::string> trim = {
				".json",
				".geojson",
				".geobuf",
				".mbtiles",
				".csv",
				".gz",
			};

			// Trim .json or .mbtiles from the name
			bool again = true;
			while (again) {
				again = false;
				for (size_t i = 0; i < trim.size(); i++) {
					if (trunc.size() > trim[i].size() && trunc.substr(trunc.size() - trim[i].size()) == trim[i]) {
						trunc = trunc.substr(0, trunc.size() - trim[i].size());
						again = true;
					}
				}
			}

			// Trim out characters that can't be part of selector
			std::string out;
			for (size_t p = 0; p < trunc.size(); p++) {
				if (isalpha(trunc[p]) || isdigit(trunc[p]) || trunc[p] == '_' || (trunc[p] & 0x80) != 0) {
					out.append(trunc, p, 1);
				}
			}

			sources[l].layer = out;
			if (sources[l].layer.size() == 0 || check_utf8(out).size() != 0) {
				sources[l].layer = "unknown" + std::to_string(l);
			}

			if (!quiet) {
				fprintf(stderr, "For layer %d, using name \"%s\"\n", (int) l, sources[l].layer.c_str());
			}
		}
	}

	std::map<std::string, layermap_entry> layermap;
	for (size_t l = 0; l < nlayers; l++) {
		layermap_entry e = layermap_entry(l);
		e.description = sources[l].description;
		layermap.insert(std::pair<std::string, layermap_entry>(sources[l].layer, e));
	}

	std::vector<std::map<std::string, layermap_entry> > layermaps;
	for (size_t l = 0; l < CPUS; l++) {
		layermaps.push_back(layermap);
	}

	long overall_offset = 0;
	double dist_sum = 0;
	size_t dist_count = 0;

	int files_open_before_reading = open("/dev/null", O_RDONLY | O_CLOEXEC);
	if (files_open_before_reading < 0) {
		perror("open /dev/null");
		exit(EXIT_FAILURE);
	}
	if (close(files_open_before_reading) != 0) {
		perror("close");
		exit(EXIT_FAILURE);
	}

	size_t nsources = sources.size();
	for (size_t source = 0; source < nsources; source++) {
		std::string reading;
		int fd;

		if (sources[source].file.size() == 0) {
			reading = "standard input";
			fd = 0;
		} else {
			reading = sources[source].file;
			fd = open(sources[source].file.c_str(), O_RDONLY, O_CLOEXEC);
			if (fd < 0) {
				perror(sources[source].file.c_str());
				continue;
			}
		}

		auto a = layermap.find(sources[source].layer);
		if (a == layermap.end()) {
			fprintf(stderr, "Internal error: couldn't find layer %s", sources[source].layer.c_str());
			exit(EXIT_FAILURE);
		}
		size_t layer = a->second.id;

		if (sources[source].format == "geobuf" || (sources[source].file.size() > 7 && sources[source].file.substr(sources[source].file.size() - 7) == std::string(".geobuf"))) {
			struct stat st;
			if (fstat(fd, &st) != 0) {
				perror("fstat");
				perror(sources[source].file.c_str());
				exit(EXIT_FAILURE);
			}

			char *map = (char *) mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
			if (map == MAP_FAILED) {
				fprintf(stderr, "%s: mmap: %s: %s\n", *av, reading.c_str(), strerror(errno));
				exit(EXIT_FAILURE);
			}

			std::atomic<long long> layer_seq[CPUS];
			double dist_sums[CPUS];
			size_t dist_counts[CPUS];
			std::vector<struct serialization_state> sst;
			sst.resize(CPUS);

			for (size_t i = 0; i < CPUS; i++) {
				layer_seq[i] = overall_offset;
				dist_sums[i] = 0;
				dist_counts[i] = 0;

				sst[i].fname = reading.c_str();
				sst[i].line = 0;
				sst[i].layer_seq = &layer_seq[i];
				sst[i].progress_seq = &progress_seq;
				sst[i].readers = &readers;
				sst[i].segment = i;
				sst[i].initial_x = &initial_x[i];
				sst[i].initial_y = &initial_y[i];
				sst[i].initialized = &initialized[i];
				sst[i].dist_sum = &dist_sums[i];
				sst[i].dist_count = &dist_counts[i];
				sst[i].want_dist = guess_maxzoom;
				sst[i].maxzoom = maxzoom;
				sst[i].filters = prefilter != NULL || postfilter != NULL;
				sst[i].uses_gamma = uses_gamma;
				sst[i].layermap = &layermaps[i];
				sst[i].exclude = exclude;
				sst[i].include = include;
				sst[i].exclude_all = exclude_all;
				sst[i].basezoom = basezoom;
				sst[i].attribute_types = attribute_types;
			}

			parse_geobuf(&sst, map, st.st_size, layer, sources[layer].layer);

			for (size_t i = 0; i < CPUS; i++) {
				dist_sum += dist_sums[i];
				dist_count += dist_counts[i];
			}

			if (munmap(map, st.st_size) != 0) {
				perror("munmap source file");
				exit(EXIT_FAILURE);
			}
			if (close(fd) != 0) {
				perror("close");
				exit(EXIT_FAILURE);
			}

			overall_offset = layer_seq[0];
			checkdisk(&readers);
			continue;
		}

		if (sources[source].format == "csv" || (sources[source].file.size() > 4 && sources[source].file.substr(sources[source].file.size() - 4) == std::string(".csv"))) {
			std::atomic<long long> layer_seq[CPUS];
			double dist_sums[CPUS];
			size_t dist_counts[CPUS];

			std::vector<struct serialization_state> sst;
			sst.resize(CPUS);

			// XXX factor out this duplicated setup
			for (size_t i = 0; i < CPUS; i++) {
				layer_seq[i] = overall_offset;
				dist_sums[i] = 0;
				dist_counts[i] = 0;

				sst[i].fname = reading.c_str();
				sst[i].line = 0;
				sst[i].layer_seq = &layer_seq[i];
				sst[i].progress_seq = &progress_seq;
				sst[i].readers = &readers;
				sst[i].segment = i;
				sst[i].initial_x = &initial_x[i];
				sst[i].initial_y = &initial_y[i];
				sst[i].initialized = &initialized[i];
				sst[i].dist_sum = &dist_sums[i];
				sst[i].dist_count = &dist_counts[i];
				sst[i].want_dist = guess_maxzoom;
				sst[i].maxzoom = maxzoom;
				sst[i].filters = prefilter != NULL || postfilter != NULL;
				sst[i].uses_gamma = uses_gamma;
				sst[i].layermap = &layermaps[i];
				sst[i].exclude = exclude;
				sst[i].include = include;
				sst[i].exclude_all = exclude_all;
				sst[i].basezoom = basezoom;
				sst[i].attribute_types = attribute_types;
			}

			parse_geocsv(sst, sources[source].file, layer, sources[layer].layer);

			if (close(fd) != 0) {
				perror("close");
				exit(EXIT_FAILURE);
			}

			overall_offset = layer_seq[0];
			checkdisk(&readers);
			continue;
		}

		struct stat st;
		char *map = NULL;
		off_t off = 0;

		int read_parallel_this = read_parallel ? '\n' : 0;

		if (!(sources[source].file.size() > 3 && sources[source].file.substr(sources[source].file.size() - 3) == std::string(".gz"))) {
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

		if (map != NULL && map != MAP_FAILED && st.st_size - off > 0) {
			if (map[0] == 0x1E) {
				read_parallel_this = 0x1E;
			}

			if (!read_parallel_this) {
				// Not a GeoJSON text sequence, so unmap and read serially

				if (munmap(map, st.st_size - off) != 0) {
					perror("munmap source file");
					exit(EXIT_FAILURE);
				}

				map = NULL;
			}
		}

		if (map != NULL && map != MAP_FAILED && read_parallel_this) {
			do_read_parallel(map, st.st_size - off, overall_offset, reading.c_str(), &readers, &progress_seq, exclude, include, exclude_all, basezoom, layer, &layermaps, initialized, initial_x, initial_y, maxzoom, sources[layer].layer, uses_gamma, attribute_types, read_parallel_this, &dist_sum, &dist_count, guess_maxzoom, prefilter != NULL || postfilter != NULL);
			overall_offset += st.st_size - off;
			checkdisk(&readers);

			if (munmap(map, st.st_size - off) != 0) {
				perror("munmap source file");
				exit(EXIT_FAILURE);
			}

			if (close(fd) != 0) {
				perror("close input file");
				exit(EXIT_FAILURE);
			}
		} else {
			STREAM *fp = streamfdopen(fd, "r", sources[layer].file);
			if (fp == NULL) {
				perror(sources[layer].file.c_str());
				if (close(fd) != 0) {
					perror("close source file");
					exit(EXIT_FAILURE);
				}
				continue;
			}

			int c = fp->peekc();
			if (c == 0x1E) {
				read_parallel_this = 0x1E;
			}

			if (read_parallel_this) {
				// Serial reading of chunks that are then parsed in parallel

				char readname[strlen(tmpdir) + strlen("/read.XXXXXXXX") + 1];
				sprintf(readname, "%s%s", tmpdir, "/read.XXXXXXXX");
				int readfd = mkstemp_cloexec(readname);
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

				std::atomic<int> is_parsing(0);
				long long ahead = 0;
				long long initial_offset = overall_offset;
				pthread_t parallel_parser;
				bool parser_created = false;

#define READ_BUF 2000
#define PARSE_MIN 10000000
#define PARSE_MAX (1LL * 1024 * 1024 * 1024)

				char buf[READ_BUF];
				int n;

				while ((n = fp->read(buf, READ_BUF)) > 0) {
					fwrite_check(buf, sizeof(char), n, readfp, reading.c_str());
					ahead += n;

					if (buf[n - 1] == read_parallel_this && ahead > PARSE_MIN) {
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
							start_parsing(readfd, streamfpopen(readfp), initial_offset, ahead, &is_parsing, &parallel_parser, parser_created, reading.c_str(), &readers, &progress_seq, exclude, include, exclude_all, basezoom, layer, layermaps, initialized, initial_x, initial_y, maxzoom, sources[layer].layer, gamma != 0, attribute_types, read_parallel_this, &dist_sum, &dist_count, guess_maxzoom, prefilter != NULL || postfilter != NULL);

							initial_offset += ahead;
							overall_offset += ahead;
							checkdisk(&readers);
							ahead = 0;

							sprintf(readname, "%s%s", tmpdir, "/read.XXXXXXXX");
							readfd = mkstemp_cloexec(readname);
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
					start_parsing(readfd, streamfpopen(readfp), initial_offset, ahead, &is_parsing, &parallel_parser, parser_created, reading.c_str(), &readers, &progress_seq, exclude, include, exclude_all, basezoom, layer, layermaps, initialized, initial_x, initial_y, maxzoom, sources[layer].layer, gamma != 0, attribute_types, read_parallel_this, &dist_sum, &dist_count, guess_maxzoom, prefilter != NULL || postfilter != NULL);

					if (parser_created) {
						if (pthread_join(parallel_parser, NULL) != 0) {
							perror("pthread_join 1133");
						}
						parser_created = false;
					}

					overall_offset += ahead;
					checkdisk(&readers);
				}
			} else {
				// Plain serial reading

				std::atomic<long long> layer_seq(overall_offset);
				json_pull *jp = fp->json_begin();
				struct serialization_state sst;

				sst.fname = reading.c_str();
				sst.line = 0;
				sst.layer_seq = &layer_seq;
				sst.progress_seq = &progress_seq;
				sst.readers = &readers;
				sst.segment = 0;
				sst.initial_x = &initial_x[0];
				sst.initial_y = &initial_y[0];
				sst.initialized = &initialized[0];
				sst.dist_sum = &dist_sum;
				sst.dist_count = &dist_count;
				sst.want_dist = guess_maxzoom;
				sst.maxzoom = maxzoom;
				sst.filters = prefilter != NULL || postfilter != NULL;
				sst.uses_gamma = uses_gamma;
				sst.layermap = &layermaps[0];
				sst.exclude = exclude;
				sst.include = include;
				sst.exclude_all = exclude_all;
				sst.basezoom = basezoom;
				sst.attribute_types = attribute_types;

				parse_json(&sst, jp, layer, sources[layer].layer);
				json_end(jp);
				overall_offset = layer_seq;
				checkdisk(&readers);
			}

			if (fp->fclose() != 0) {
				perror("fclose input");
				exit(EXIT_FAILURE);
			}
		}
	}

	int files_open_after_reading = open("/dev/null", O_RDONLY | O_CLOEXEC);
	if (files_open_after_reading < 0) {
		perror("open /dev/null");
		exit(EXIT_FAILURE);
	}
	if (close(files_open_after_reading) != 0) {
		perror("close");
		exit(EXIT_FAILURE);
	}

	if (files_open_after_reading > files_open_before_reading) {
		fprintf(stderr, "Internal error: Files left open after reading input. (%d vs %d)\n",
			files_open_before_reading, files_open_after_reading);
		ret = EXIT_FAILURE;
	}

	if (!quiet) {
		fprintf(stderr, "                              \r");
		//     (stderr, "Read 10000.00 million features\r", *progress_seq / 1000000.0);
	}

	for (size_t i = 0; i < CPUS; i++) {
		if (fclose(readers[i].metafile) != 0) {
			perror("fclose meta");
			exit(EXIT_FAILURE);
		}
		if (fclose(readers[i].geomfile) != 0) {
			perror("fclose geom");
			exit(EXIT_FAILURE);
		}
		if (fclose(readers[i].indexfile) != 0) {
			perror("fclose index");
			exit(EXIT_FAILURE);
		}
		memfile_close(readers[i].treefile);

		if (fstat(readers[i].geomfd, &readers[i].geomst) != 0) {
			perror("stat geom\n");
			exit(EXIT_FAILURE);
		}
		if (fstat(readers[i].metafd, &readers[i].metast) != 0) {
			perror("stat meta\n");
			exit(EXIT_FAILURE);
		}
	}

	// Create a combined string pool and a combined metadata file
	// but keep track of the offsets into it since we still need
	// segment+offset to find the data.

	// 2 * CPUS: One per input thread, one per tiling thread
	long long pool_off[2 * CPUS];
	long long meta_off[2 * CPUS];
	for (size_t i = 0; i < 2 * CPUS; i++) {
		pool_off[i] = meta_off[i] = 0;
	}

	char poolname[strlen(tmpdir) + strlen("/pool.XXXXXXXX") + 1];
	sprintf(poolname, "%s%s", tmpdir, "/pool.XXXXXXXX");

	int poolfd = mkstemp_cloexec(poolname);
	if (poolfd < 0) {
		perror(poolname);
		exit(EXIT_FAILURE);
	}

	FILE *poolfile = fopen_oflag(poolname, "wb", O_WRONLY | O_CLOEXEC);
	if (poolfile == NULL) {
		perror(poolname);
		exit(EXIT_FAILURE);
	}

	unlink(poolname);

	char metaname[strlen(tmpdir) + strlen("/meta.XXXXXXXX") + 1];
	sprintf(metaname, "%s%s", tmpdir, "/meta.XXXXXXXX");

	int metafd = mkstemp_cloexec(metaname);
	if (metafd < 0) {
		perror(metaname);
		exit(EXIT_FAILURE);
	}

	FILE *metafile = fopen_oflag(metaname, "wb", O_WRONLY | O_CLOEXEC);
	if (metafile == NULL) {
		perror(metaname);
		exit(EXIT_FAILURE);
	}

	unlink(metaname);

	std::atomic<long long> metapos(0);
	std::atomic<long long> poolpos(0);

	for (size_t i = 0; i < CPUS; i++) {
		if (readers[i].metapos > 0) {
			void *map = mmap(NULL, readers[i].metapos, PROT_READ, MAP_PRIVATE, readers[i].metafd, 0);
			if (map == MAP_FAILED) {
				perror("mmap unmerged meta");
				exit(EXIT_FAILURE);
			}
			madvise(map, readers[i].metapos, MADV_SEQUENTIAL);
			madvise(map, readers[i].metapos, MADV_WILLNEED);
			if (fwrite(map, readers[i].metapos, 1, metafile) != 1) {
				perror("Reunify meta");
				exit(EXIT_FAILURE);
			}
			madvise(map, readers[i].metapos, MADV_DONTNEED);
			if (munmap(map, readers[i].metapos) != 0) {
				perror("unmap unmerged meta");
			}
		}

		meta_off[i] = metapos;
		metapos += readers[i].metapos;
		if (close(readers[i].metafd) != 0) {
			perror("close unmerged meta");
		}

		if (readers[i].poolfile->off > 0) {
			if (fwrite(readers[i].poolfile->map, readers[i].poolfile->off, 1, poolfile) != 1) {
				perror("Reunify string pool");
				exit(EXIT_FAILURE);
			}
		}

		pool_off[i] = poolpos;
		poolpos += readers[i].poolfile->off;
		memfile_close(readers[i].poolfile);
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

	int indexfd = mkstemp_cloexec(indexname);
	if (indexfd < 0) {
		perror(indexname);
		exit(EXIT_FAILURE);
	}
	FILE *indexfile = fopen_oflag(indexname, "wb", O_WRONLY | O_CLOEXEC);
	if (indexfile == NULL) {
		perror(indexname);
		exit(EXIT_FAILURE);
	}

	unlink(indexname);

	char geomname[strlen(tmpdir) + strlen("/geom.XXXXXXXX") + 1];
	sprintf(geomname, "%s%s", tmpdir, "/geom.XXXXXXXX");

	int geomfd = mkstemp_cloexec(geomname);
	if (geomfd < 0) {
		perror(geomname);
		exit(EXIT_FAILURE);
	}
	FILE *geomfile = fopen_oflag(geomname, "wb", O_WRONLY | O_CLOEXEC);
	if (geomfile == NULL) {
		perror(geomname);
		exit(EXIT_FAILURE);
	}
	unlink(geomname);

	unsigned iz = 0, ix = 0, iy = 0;
	choose_first_zoom(file_bbox, readers, &iz, &ix, &iy, minzoom, buffer);

	if (justx >= 0) {
		iz = minzoom;
		ix = justx;
		iy = justy;
	}

	std::atomic<long long> geompos(0);

	/* initial tile is 0/0/0 */
	serialize_int(geomfile, iz, &geompos, fname);
	serialize_uint(geomfile, ix, &geompos, fname);
	serialize_uint(geomfile, iy, &geompos, fname);

	radix(readers, CPUS, geomfile, indexfile, tmpdir, &geompos, maxzoom, basezoom, droprate, gamma);

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
	std::atomic<long long> indexpos(indexst.st_size);
	progress_seq = indexpos / sizeof(struct index);

	last_progress = 0;
	if (!quiet) {
		long long s = progress_seq;
		long long geompos_print = geompos;
		long long metapos_print = metapos;
		long long poolpos_print = poolpos;
		fprintf(stderr, "%lld features, %lld bytes of geometry, %lld bytes of separate metadata, %lld bytes of string pool\n", s, geompos_print, metapos_print, poolpos_print);
	}

	if (indexpos == 0) {
		fprintf(stderr, "Did not read any valid geometries\n");
		if (outdb != NULL) {
			mbtiles_close(outdb, pgm);
		}
		exit(EXIT_FAILURE);
	}

	struct index *map = (struct index *) mmap(NULL, indexpos, PROT_READ, MAP_PRIVATE, indexfd, 0);
	if (map == MAP_FAILED) {
		perror("mmap index for basezoom");
		exit(EXIT_FAILURE);
	}
	madvise(map, indexpos, MADV_SEQUENTIAL);
	madvise(map, indexpos, MADV_WILLNEED);
	long long indices = indexpos / sizeof(struct index);
	bool fix_dropping = false;

	if (guess_maxzoom) {
		double sum = 0;
		size_t count = 0;

		long long progress = -1;
		long long ip;
		for (ip = 1; ip < indices; ip++) {
			if (map[ip].ix != map[ip - 1].ix) {
				count++;
				sum += log(map[ip].ix - map[ip - 1].ix);
			}

			long long nprogress = 100 * ip / indices;
			if (nprogress != progress) {
				progress = nprogress;
				if (!quiet && !quiet_progress && progress_time()) {
					fprintf(stderr, "Maxzoom: %lld%% \r", progress);
				}
			}
		}

		if (count == 0 && dist_count == 0) {
			fprintf(stderr, "Can't guess maxzoom (-zg) without at least two distinct feature locations\n");
			if (outdb != NULL) {
				mbtiles_close(outdb, pgm);
			}
			exit(EXIT_FAILURE);
		}

		if (count > 0) {
			// Geometric mean is appropriate because distances between features
			// are typically lognormally distributed
			double avg = exp(sum / count);

			// Convert approximately from tile units to feet
			double dist_ft = sqrt(avg) / 33;
			// Factor of 8 (3 zooms) beyond minimum required to distinguish features
			double want = dist_ft / 8;

			maxzoom = ceil(log(360 / (.00000274 * want)) / log(2) - full_detail);
			if (maxzoom < 0) {
				maxzoom = 0;
			}
			if (maxzoom > 32 - full_detail) {
				maxzoom = 32 - full_detail;
			}
			if (maxzoom > 33 - low_detail) {  // that is, maxzoom - 1 > 32 - low_detail
				maxzoom = 33 - low_detail;
			}

			if (!quiet) {
				fprintf(stderr, "Choosing a maxzoom of -z%d for features about %d feet (%d meters) apart\n", maxzoom, (int) ceil(dist_ft), (int) ceil(dist_ft / 3.28084));
			}

			bool changed = false;
			while (maxzoom < 32 - full_detail && maxzoom < 33 - low_detail && cluster_distance > 0) {
				unsigned long long zoom_mingap = ((1LL << (32 - maxzoom)) / 256 * cluster_distance) * ((1LL << (32 - maxzoom)) / 256 * cluster_distance);
				if (avg > zoom_mingap) {
					break;
				}

				maxzoom++;
				changed = true;
			}
			if (changed) {
				printf("Choosing a maxzoom of -z%d to keep most features distinct with cluster distance %d\n", maxzoom, cluster_distance);
			}
		}

		if (dist_count != 0) {
			double want2 = exp(dist_sum / dist_count) / 8;
			int mz = ceil(log(360 / (.00000274 * want2)) / log(2) - full_detail);

			if (mz < 0) {
				mz = 0;
			}
			if (mz > 32 - full_detail) {
				mz = 32 - full_detail;
			}
			if (mz > 33 - low_detail) {  // that is, mz - 1 > 32 - low_detail
				mz = 33 - low_detail;
			}

			if (mz > maxzoom || count <= 0) {
				if (!quiet) {
					fprintf(stderr, "Choosing a maxzoom of -z%d for resolution of about %d feet (%d meters) within features\n", mz, (int) exp(dist_sum / dist_count), (int) (exp(dist_sum / dist_count) / 3.28084));
				}
				maxzoom = mz;
			}
		}

		if (maxzoom < minzoom) {
			fprintf(stderr, "Can't use %d for maxzoom because minzoom is %d\n", maxzoom, minzoom);
			maxzoom = minzoom;
		}

		fix_dropping = true;

		if (basezoom == -1) {
			basezoom = maxzoom;
		}
	}

	if (basezoom < 0 || droprate < 0) {
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

		long long ip;
		for (ip = 0; ip < indices; ip++) {
			unsigned xx, yy;
			decode_index(map[ip].ix, &xx, &yy);

			long long nprogress = 100 * ip / indices;
			if (nprogress != progress) {
				progress = nprogress;
				if (!quiet && !quiet_progress && progress_time()) {
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

				if (manage_gap(map[ip].ix, &tile[z].previndex, scale, gamma, &tile[z].gap)) {
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

			if (!quiet) {
				fprintf(stderr, "Choosing a base zoom of -B%d to keep %lld features in tile %d/%u/%u.\n", basezoom, max[basezoom].count, basezoom, max[basezoom].x, max[basezoom].y);
			}
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
					if (!quiet) {
						fprintf(stderr, "Choosing a drop rate of -r%f to get from %lld to %lld in %d zooms\n", droprate, max[maxzoom].count, max[0].count, maxzoom);
					}
				}
			}

			basezoom = 0;
			for (z = 0; z <= maxzoom; z++) {
				double zoomdiff = log((long double) max[z].count / max_features) / log(droprate);
				if (zoomdiff + z > basezoom) {
					basezoom = ceil(zoomdiff + z);
				}
			}

			if (!quiet) {
				fprintf(stderr, "Choosing a base zoom of -B%d to keep %f features in tile %d/%u/%u.\n", basezoom, max[maxzoom].count * exp(log(droprate) * (maxzoom - basezoom)), maxzoom, max[maxzoom].x, max[maxzoom].y);
			}
		} else if (droprate < 0) {
			droprate = 1;

			for (z = basezoom - 1; z >= 0; z--) {
				double interval = exp(log(droprate) * (basezoom - z));

				if (max[z].count / interval >= max_features) {
					interval = (long double) max[z].count / max_features;
					droprate = exp(log(interval) / (basezoom - z));
					interval = exp(log(droprate) * (basezoom - z));

					if (!quiet) {
						fprintf(stderr, "Choosing a drop rate of -r%f to keep %f features in tile %d/%u/%u.\n", droprate, max[z].count / interval, z, max[z].x, max[z].y);
					}
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
				if (!quiet) {
					fprintf(stderr, "With gamma, effective base zoom is 0, so no effective drop rate\n");
				}
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

				if (!quiet) {
					fprintf(stderr, "With gamma, effective base zoom of %d, effective drop rate of %f\n", effective, eff_drop);
				}
			}
		}

		fix_dropping = true;
	}

	if (fix_dropping) {
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

		for (long long ip = 0; ip < indices; ip++) {
			if (ip > 0 && map[ip].start != map[ip - 1].end) {
				fprintf(stderr, "Mismatched index at %lld: %lld vs %lld\n", ip, map[ip].start, map[ip].end);
			}
			int feature_minzoom = calc_feature_minzoom(&map[ip], ds, maxzoom, gamma);
			geom[map[ip].end - 1] = feature_minzoom;
		}

		munmap(geom, geomst.st_size);
	}

	madvise(map, indexpos, MADV_DONTNEED);
	munmap(map, indexpos);

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

	std::atomic<unsigned> midx(0);
	std::atomic<unsigned> midy(0);
	int written = traverse_zooms(fd, size, meta, stringpool, &midx, &midy, maxzoom, minzoom, outdb, outdir, buffer, fname, tmpdir, gamma, full_detail, low_detail, min_detail, meta_off, pool_off, initial_x, initial_y, simplification, layermaps, prefilter, postfilter, attribute_accum, filter);

	if (maxzoom != written) {
		if (written > minzoom) {
			fprintf(stderr, "\n\n\n*** NOTE TILES ONLY COMPLETE THROUGH ZOOM %d ***\n\n\n", written);
			maxzoom = written;
			ret = 100;
		} else {
			fprintf(stderr, "%s: No zoom levels were successfully written\n", *av);
			exit(EXIT_FAILURE);
		}
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

	for (auto ai = merged_lm.begin(); ai != merged_lm.end(); ++ai) {
		ai->second.minzoom = minzoom;
		ai->second.maxzoom = maxzoom;
	}

	mbtiles_write_metadata(outdb, outdir, fname, minzoom, maxzoom, minlat, minlon, maxlat, maxlon, midlat, midlon, forcetable, attribution, merged_lm, true, description, !prevent[P_TILE_STATS], attribute_descriptions, "tippecanoe", commandline);

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

void set_attribute_type(std::map<std::string, int> &attribute_types, const char *arg) {
	const char *s = strchr(arg, ':');
	if (s == NULL) {
		fprintf(stderr, "-T%s option must be in the form -Tname:type\n", arg);
		exit(EXIT_FAILURE);
	}

	std::string name = std::string(arg, s - arg);
	std::string type = std::string(s + 1);
	int t = -1;

	if (type == "int") {
		t = mvt_int;
	} else if (type == "float") {
		t = mvt_float;
	} else if (type == "string") {
		t = mvt_string;
	} else if (type == "bool") {
		t = mvt_bool;
	} else {
		fprintf(stderr, "Attribute type (%s) must be int, float, string, or bool\n", type.c_str());
		exit(EXIT_FAILURE);
	}

	attribute_types.insert(std::pair<std::string, int>(name, t));
}

void set_attribute_accum(std::map<std::string, attribute_op> &attribute_accum, const char *arg) {
	const char *s = strchr(arg, ':');
	if (s == NULL) {
		fprintf(stderr, "-E%s option must be in the form -Ename:method\n", arg);
		exit(EXIT_FAILURE);
	}

	std::string name = std::string(arg, s - arg);
	std::string type = std::string(s + 1);
	attribute_op t;

	if (type == "sum") {
		t = op_sum;
	} else if (type == "product") {
		t = op_product;
	} else if (type == "mean") {
		t = op_mean;
	} else if (type == "max") {
		t = op_max;
	} else if (type == "min") {
		t = op_min;
	} else if (type == "concat") {
		t = op_concat;
	} else if (type == "comma") {
		t = op_comma;
	} else {
		fprintf(stderr, "Attribute method (%s) must be sum, product, mean, max, min, concat, or comma\n", type.c_str());
		exit(EXIT_FAILURE);
	}

	attribute_accum.insert(std::pair<std::string, attribute_op>(name, t));
}

void parse_json_source(const char *arg, struct source &src) {
	json_pull *jp = json_begin_string(arg);
	json_object *o = json_read_tree(jp);

	if (o == NULL) {
		fprintf(stderr, "%s: -L%s: %s\n", *av, arg, jp->error);
		exit(EXIT_FAILURE);
	}

	if (o->type != JSON_HASH) {
		fprintf(stderr, "%s: -L%s: not a JSON object\n", *av, arg);
		exit(EXIT_FAILURE);
	}

	json_object *fname = json_hash_get(o, "file");
	if (fname == NULL || fname->type != JSON_STRING) {
		fprintf(stderr, "%s: -L%s: requires \"file\": filename\n", *av, arg);
		exit(EXIT_FAILURE);
	}

	src.file = std::string(fname->string);

	json_object *layer = json_hash_get(o, "layer");
	if (layer != NULL && layer->type == JSON_STRING) {
		src.layer = std::string(layer->string);
	}

	json_object *description = json_hash_get(o, "description");
	if (description != NULL && description->type == JSON_STRING) {
		src.description = std::string(description->string);
	}

	json_object *format = json_hash_get(o, "format");
	if (format != NULL && format->type == JSON_STRING) {
		src.format = std::string(format->string);
	}

	json_free(o);
	json_end(jp);
}

int main(int argc, char **argv) {
#ifdef MTRACE
	mtrace();
#endif

	av = argv;
	init_cpus();

	extern int optind;
	extern char *optarg;
	int i;

	char *name = NULL;
	char *description = NULL;
	char *layername = NULL;
	char *out_mbtiles = NULL;
	char *out_dir = NULL;
	sqlite3 *outdb = NULL;
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
	const char *prefilter = NULL;
	const char *postfilter = NULL;
	bool guess_maxzoom = false;

	std::set<std::string> exclude, include;
	std::map<std::string, int> attribute_types;
	std::map<std::string, attribute_op> attribute_accum;
	std::map<std::string, std::string> attribute_descriptions;
	int exclude_all = 0;
	int read_parallel = 0;
	int files_open_at_start;
	json_object *filter = NULL;

	for (i = 0; i < 256; i++) {
		prevent[i] = 0;
		additional[i] = 0;
	}

	static struct option long_options_orig[] = {
		{"Output tileset", 0, 0, 0},
		{"output", required_argument, 0, 'o'},
		{"output-to-directory", required_argument, 0, 'e'},
		{"force", no_argument, 0, 'f'},
		{"allow-existing", no_argument, 0, 'F'},

		{"Tileset description and attribution", 0, 0, 0},
		{"name", required_argument, 0, 'n'},
		{"attribution", required_argument, 0, 'A'},
		{"description", required_argument, 0, 'N'},

		{"Input files and layer names", 0, 0, 0},
		{"layer", required_argument, 0, 'l'},
		{"named-layer", required_argument, 0, 'L'},

		{"Parallel processing of input", 0, 0, 0},
		{"read-parallel", no_argument, 0, 'P'},

		{"Projection of input", 0, 0, 0},
		{"projection", required_argument, 0, 's'},

		{"Zoom levels", 0, 0, 0},
		{"maximum-zoom", required_argument, 0, 'z'},
		{"minimum-zoom", required_argument, 0, 'Z'},
		{"extend-zooms-if-still-dropping", no_argument, &additional[A_EXTEND_ZOOMS], 1},
		{"one-tile", required_argument, 0, 'R'},

		{"Tile resolution", 0, 0, 0},
		{"full-detail", required_argument, 0, 'd'},
		{"low-detail", required_argument, 0, 'D'},
		{"minimum-detail", required_argument, 0, 'm'},

		{"Filtering feature attributes", 0, 0, 0},
		{"exclude", required_argument, 0, 'x'},
		{"include", required_argument, 0, 'y'},
		{"exclude-all", no_argument, 0, 'X'},

		{"Modifying feature attributes", 0, 0, 0},
		{"attribute-type", required_argument, 0, 'T'},
		{"attribute-description", required_argument, 0, 'Y'},
		{"accumulate-attribute", required_argument, 0, 'E'},
		{"empty-csv-columns-are-null", no_argument, &prevent[P_EMPTY_CSV_COLUMNS], 1},
		{"convert-stringified-ids-to-numbers", no_argument, &additional[A_CONVERT_NUMERIC_IDS], 1},
		{"use-attribute-for-id", required_argument, 0, '~'},

		{"Filtering features by attributes", 0, 0, 0},
		{"feature-filter-file", required_argument, 0, 'J'},
		{"feature-filter", required_argument, 0, 'j'},

		{"Dropping a fixed fraction of features by zoom level", 0, 0, 0},
		{"drop-rate", required_argument, 0, 'r'},
		{"base-zoom", required_argument, 0, 'B'},
		{"drop-lines", no_argument, &additional[A_LINE_DROP], 1},
		{"drop-polygons", no_argument, &additional[A_POLYGON_DROP], 1},
		{"cluster-distance", required_argument, 0, 'K'},

		{"Dropping or merging a fraction of features to keep under tile size limits", 0, 0, 0},
		{"drop-densest-as-needed", no_argument, &additional[A_DROP_DENSEST_AS_NEEDED], 1},
		{"drop-fraction-as-needed", no_argument, &additional[A_DROP_FRACTION_AS_NEEDED], 1},
		{"drop-smallest-as-needed", no_argument, &additional[A_DROP_SMALLEST_AS_NEEDED], 1},
		{"coalesce-densest-as-needed", no_argument, &additional[A_COALESCE_DENSEST_AS_NEEDED], 1},
		{"coalesce-fraction-as-needed", no_argument, &additional[A_COALESCE_FRACTION_AS_NEEDED], 1},
		{"coalesce-smallest-as-needed", no_argument, &additional[A_COALESCE_SMALLEST_AS_NEEDED], 1},
		{"force-feature-limit", no_argument, &prevent[P_DYNAMIC_DROP], 1},
		{"cluster-densest-as-needed", no_argument, &additional[A_CLUSTER_DENSEST_AS_NEEDED], 1},

		{"Dropping tightly overlapping features", 0, 0, 0},
		{"gamma", required_argument, 0, 'g'},
		{"increase-gamma-as-needed", no_argument, &additional[A_INCREASE_GAMMA_AS_NEEDED], 1},

		{"Line and polygon simplification", 0, 0, 0},
		{"simplification", required_argument, 0, 'S'},
		{"no-line-simplification", no_argument, &prevent[P_SIMPLIFY], 1},
		{"simplify-only-low-zooms", no_argument, &prevent[P_SIMPLIFY_LOW], 1},
		{"no-tiny-polygon-reduction", no_argument, &prevent[P_TINY_POLYGON_REDUCTION], 1},
		{"no-simplification-of-shared-nodes", no_argument, &prevent[P_SIMPLIFY_SHARED_NODES], 1},

		{"Attempts to improve shared polygon boundaries", 0, 0, 0},
		{"detect-shared-borders", no_argument, &additional[A_DETECT_SHARED_BORDERS], 1},
		{"extract-borders", no_argument, &additional[A_EXTRACT_BORDERS], 1},
		{"grid-low-zooms", no_argument, &additional[A_GRID_LOW_ZOOMS], 1},

		{"Controlling clipping to tile boundaries", 0, 0, 0},
		{"buffer", required_argument, 0, 'b'},
		{"no-clipping", no_argument, &prevent[P_CLIPPING], 1},
		{"no-duplication", no_argument, &prevent[P_DUPLICATION], 1},

		{"Reordering features within each tile", 0, 0, 0},
		{"preserve-input-order", no_argument, &prevent[P_INPUT_ORDER], 1},
		{"reorder", no_argument, &additional[A_REORDER], 1},
		{"coalesce", no_argument, &additional[A_COALESCE], 1},
		{"reverse", no_argument, &additional[A_REVERSE], 1},
		{"hilbert", no_argument, &additional[A_HILBERT], 1},

		{"Adding calculated attributes", 0, 0, 0},
		{"calculate-feature-density", no_argument, &additional[A_CALCULATE_FEATURE_DENSITY], 1},
		{"generate-ids", no_argument, &additional[A_GENERATE_IDS], 1},

		{"Trying to correct bad source geometry", 0, 0, 0},
		{"detect-longitude-wraparound", no_argument, &additional[A_DETECT_WRAPAROUND], 1},
		{"use-source-polygon-winding", no_argument, &prevent[P_USE_SOURCE_POLYGON_WINDING], 1},
		{"reverse-source-polygon-winding", no_argument, &prevent[P_REVERSE_SOURCE_POLYGON_WINDING], 1},
		{"clip-bounding-box", required_argument, 0, '~'},

		{"Filtering tile contents", 0, 0, 0},
		{"prefilter", required_argument, 0, 'C'},
		{"postfilter", required_argument, 0, 'c'},

		{"Setting or disabling tile size limits", 0, 0, 0},
		{"maximum-tile-bytes", required_argument, 0, 'M'},
		{"maximum-tile-features", required_argument, 0, 'O'},
		{"no-feature-limit", no_argument, &prevent[P_FEATURE_LIMIT], 1},
		{"no-tile-size-limit", no_argument, &prevent[P_KILOBYTE_LIMIT], 1},
		{"no-tile-compression", no_argument, &prevent[P_TILE_COMPRESSION], 1},
		{"no-tile-stats", no_argument, &prevent[P_TILE_STATS], 1},
		{"tile-stats-attributes-limit", required_argument, 0, '~'},
		{"tile-stats-sample-values-limit", required_argument, 0, '~'},
		{"tile-stats-values-limit", required_argument, 0, '~'},

		{"Temporary storage", 0, 0, 0},
		{"temporary-directory", required_argument, 0, 't'},

		{"Progress indicator", 0, 0, 0},
		{"quiet", no_argument, 0, 'q'},
		{"no-progress-indicator", no_argument, 0, 'Q'},
		{"progress-interval", required_argument, 0, 'U'},
		{"version", no_argument, 0, 'v'},

		{"", 0, 0, 0},
		{"prevent", required_argument, 0, 'p'},
		{"additional", required_argument, 0, 'a'},
		{"check-polygons", no_argument, &additional[A_DEBUG_POLYGON], 1},
		{"no-polygon-splitting", no_argument, &prevent[P_POLYGON_SPLIT], 1},
		{"prefer-radix-sort", no_argument, &additional[A_PREFER_RADIX_SORT], 1},
		{"help", no_argument, 0, 'H'},

		{0, 0, 0, 0},
	};

	static struct option long_options[sizeof(long_options_orig) / sizeof(long_options_orig[0])];
	static char getopt_str[sizeof(long_options_orig) / sizeof(long_options_orig[0]) * 2 + 1];

	{
		size_t out = 0;
		size_t cout = 0;
		for (size_t lo = 0; long_options_orig[lo].name != NULL; lo++) {
			if (long_options_orig[lo].val != 0) {
				long_options[out++] = long_options_orig[lo];

				if (long_options_orig[lo].val > ' ') {
					getopt_str[cout++] = long_options_orig[lo].val;

					if (long_options_orig[lo].has_arg == required_argument) {
						getopt_str[cout++] = ':';
					}
				}
			}
		}
		long_options[out] = {0, 0, 0, 0};
		getopt_str[cout] = '\0';

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

	std::string commandline = format_commandline(argc, argv);

	int option_index = 0;
	while ((i = getopt_long(argc, argv, getopt_str, long_options, &option_index)) != -1) {
		switch (i) {
		case 0:
			break;

		case '~': {
			const char *opt = long_options[option_index].name;
			if (strcmp(opt, "tile-stats-attributes-limit") == 0) {
				max_tilestats_attributes = atoi(optarg);
			} else if (strcmp(opt, "tile-stats-sample-values-limit") == 0) {
				max_tilestats_sample_values = atoi(optarg);
			} else if (strcmp(opt, "tile-stats-values-limit") == 0) {
				max_tilestats_values = atoi(optarg);
			} else if (strcmp(opt, "clip-bounding-box") == 0) {
				clipbbox clip;
				if (sscanf(optarg, "%lf,%lf,%lf,%lf", &clip.lon1, &clip.lat1, &clip.lon2, &clip.lat2) == 4) {
					clipbboxes.push_back(clip);
				} else {
					fprintf(stderr, "%s: Can't parse bounding box --%s=%s\n", argv[0], opt, optarg);
					exit(EXIT_FAILURE);
				}
			} else if (strcmp(opt, "use-attribute-for-id") == 0) {
				attribute_for_id = optarg;
			} else {
				fprintf(stderr, "%s: Unrecognized option --%s\n", argv[0], opt);
				exit(EXIT_FAILURE);
			}
			break;
		}

		case 'n':
			name = optarg;
			break;

		case 'N':
			description = optarg;
			break;

		case 'l':
			layername = optarg;
			break;

		case 'A':
			attribution = optarg;
			break;

		case 'L': {
			struct source src;
			if (optarg[0] == '{') {
				parse_json_source(optarg, src);
			} else {
				char *cp = strchr(optarg, ':');
				if (cp == NULL || cp == optarg) {
					fprintf(stderr, "%s: -L requires layername:file\n", argv[0]);
					exit(EXIT_FAILURE);
				}
				src.layer = std::string(optarg).substr(0, cp - optarg);
				src.file = std::string(cp + 1);
			}
			sources.push_back(src);
			break;
		}

		case 'z':
			if (strcmp(optarg, "g") == 0) {
				maxzoom = MAX_ZOOM;
				guess_maxzoom = true;
			} else {
				maxzoom = atoi_require(optarg, "Maxzoom");
			}
			break;

		case 'Z':
			minzoom = atoi_require(optarg, "Minzoom");
			break;

		case 'R': {
			unsigned z, x, y;
			if (sscanf(optarg, "%u/%u/%u", &z, &x, &y) == 3) {
				minzoom = z;
				maxzoom = z;
				justx = x;
				justy = y;
			} else {
				fprintf(stderr, "--one-tile argument must be z/x/y\n");
				exit(EXIT_FAILURE);
			}
			break;
		}

		case 'B':
			if (strcmp(optarg, "g") == 0) {
				basezoom = -2;
			} else if (optarg[0] == 'g' || optarg[0] == 'f') {
				basezoom = -2;
				if (optarg[0] == 'g') {
					basezoom_marker_width = atof_require(optarg + 1, "Marker width");
				} else {
					basezoom_marker_width = sqrt(50000 / atof_require(optarg + 1, "Marker width"));
				}
				if (basezoom_marker_width == 0 || atof_require(optarg + 1, "Marker width") == 0) {
					fprintf(stderr, "%s: Must specify value >0 with -B%c\n", argv[0], optarg[0]);
					exit(EXIT_FAILURE);
				}
			} else {
				basezoom = atoi_require(optarg, "Basezoom");
				if (basezoom == 0 && strcmp(optarg, "0") != 0) {
					fprintf(stderr, "%s: Couldn't understand -B%s\n", argv[0], optarg);
					exit(EXIT_FAILURE);
				}
			}
			break;

		case 'K':
			cluster_distance = atoi_require(optarg, "Cluster distance");
			if (cluster_distance > 255) {
				fprintf(stderr, "%s: --cluster-distance %d is too big; limit is 255\n", argv[0], cluster_distance);
				exit(EXIT_FAILURE);
			}
			break;

		case 'd':
			full_detail = atoi_require(optarg, "Full detail");
			if (full_detail > 30) {
				// So the maximum geometry delta of just under 2 tile extents
				// is less than 2^31

				fprintf(stderr, "%s: --full-detail can be at most 30\n", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;

		case 'D':
			low_detail = atoi_require(optarg, "Low detail");
			if (low_detail > 30) {
				fprintf(stderr, "%s: --low-detail can be at most 30\n", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;

		case 'm':
			min_detail = atoi_require(optarg, "Min detail");
			break;

		case 'o':
			if (out_mbtiles != NULL) {
				fprintf(stderr, "%s: Can't specify both %s and %s as output\n", argv[0], out_mbtiles, optarg);
				exit(EXIT_FAILURE);
			}
			if (out_dir != NULL) {
				fprintf(stderr, "%s: Can't specify both %s and %s as output\n", argv[0], out_dir, optarg);
				exit(EXIT_FAILURE);
			}
			out_mbtiles = optarg;
			break;

		case 'e':
			if (out_mbtiles != NULL) {
				fprintf(stderr, "%s: Can't specify both %s and %s as output\n", argv[0], out_mbtiles, optarg);
				exit(EXIT_FAILURE);
			}
			if (out_dir != NULL) {
				fprintf(stderr, "%s: Can't specify both %s and %s as output\n", argv[0], out_dir, optarg);
				exit(EXIT_FAILURE);
			}
			out_dir = optarg;
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

		case 'Y': {
			char *cp = strchr(optarg, ':');
			if (cp == NULL || cp == optarg) {
				fprintf(stderr, "%s: -Y requires attribute:description\n", argv[0]);
				exit(EXIT_FAILURE);
			}
			std::string attrib = std::string(optarg).substr(0, cp - optarg);
			std::string desc = std::string(cp + 1);
			attribute_descriptions.insert(std::pair<std::string, std::string>(attrib, desc));
		} break;

		case 'J':
			filter = read_filter(optarg);
			break;

		case 'j':
			filter = parse_filter(optarg);
			break;

		case 'r':
			if (strcmp(optarg, "g") == 0) {
				droprate = -2;
			} else if (optarg[0] == 'g' || optarg[0] == 'f') {
				droprate = -2;
				if (optarg[0] == 'g') {
					basezoom_marker_width = atof_require(optarg + 1, "Marker width");
				} else {
					basezoom_marker_width = sqrt(50000 / atof_require(optarg + 1, "Marker width"));
				}
				if (basezoom_marker_width == 0 || atof_require(optarg + 1, "Marker width") == 0) {
					fprintf(stderr, "%s: Must specify value >0 with -r%c\n", argv[0], optarg[0]);
					exit(EXIT_FAILURE);
				}
			} else {
				droprate = atof_require(optarg, "Drop rate");
			}
			break;

		case 'b':
			buffer = atoi_require(optarg, "Buffer");
			if (buffer > 127) {
				// So the maximum geometry delta is under 2 tile extents,
				// from less than half a tile beyond one side to less than
				// half a tile beyond the other.

				fprintf(stderr, "%s: --buffer can be at most 127\n", argv[0]);
				exit(EXIT_FAILURE);
			}
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
			gamma = atof_require(optarg, "Gamma");
			break;

		case 'q':
			quiet = 1;
			break;

		case 'Q':
			quiet_progress = 1;
			break;

		case 'U':
			progress_interval = atof_require(optarg, "Progress interval");
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
			break;
		}

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
			break;
		}

		case 'v':
			fprintf(stderr, "tippecanoe %s\n", VERSION);
			exit(EXIT_SUCCESS);

		case 'P':
			read_parallel = 1;
			break;

		case 's':
			set_projection_or_exit(optarg);
			break;

		case 'S':
			simplification = atof_require(optarg, "Simplification");
			if (simplification <= 0) {
				fprintf(stderr, "%s: --simplification must be > 0\n", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;

		case 'M':
			max_tile_size = atoll_require(optarg, "Max tile size");
			break;

		case 'O':
			max_tile_features = atoll_require(optarg, "Max tile features");
			break;

		case 'c':
			postfilter = optarg;
			break;

		case 'C':
			prefilter = optarg;
			break;

		case 'T':
			set_attribute_type(attribute_types, optarg);
			break;

		case 'E':
			set_attribute_accum(attribute_accum, optarg);
			break;

		default: {
			if (i != 'H' && i != '?') {
				fprintf(stderr, "Unknown option -%c\n", i);
			}
			int width = 7 + strlen(argv[0]);
			fprintf(stderr, "Usage: %s [options] [file.json ...]", argv[0]);
			for (size_t lo = 0; long_options_orig[lo].name != NULL && strlen(long_options_orig[lo].name) > 0; lo++) {
				if (long_options_orig[lo].val == 0) {
					fprintf(stderr, "\n  %s\n        ", long_options_orig[lo].name);
					width = 8;
					continue;
				}
				if (width + strlen(long_options_orig[lo].name) + 9 >= 80) {
					fprintf(stderr, "\n        ");
					width = 8;
				}
				width += strlen(long_options_orig[lo].name) + 9;
				if (strcmp(long_options_orig[lo].name, "output") == 0) {
					fprintf(stderr, " --%s=output.mbtiles", long_options_orig[lo].name);
					width += 9;
				} else if (long_options_orig[lo].has_arg) {
					fprintf(stderr, " [--%s=...]", long_options_orig[lo].name);
				} else {
					fprintf(stderr, " [--%s]", long_options_orig[lo].name);
				}
			}
			if (width + 16 >= 80) {
				fprintf(stderr, "\n        ");
				width = 8;
			}
			fprintf(stderr, "\n");
			if (i == 'H') {
				exit(EXIT_SUCCESS);
			} else {
				exit(EXIT_FAILURE);
			}
		}
		}
	}

	if (additional[A_HILBERT]) {
		encode_index = encode_hilbert;
		decode_index = decode_hilbert;
	} else {
		encode_index = encode_quadkey;
		decode_index = decode_quadkey;
	}

	// Wait until here to project the bounding box, so that the behavior is
	// the same no matter what order the projection and bounding box are
	// specified in
	for (auto &c : clipbboxes) {
		projection->project(c.lon1, c.lat1, 32, &c.minx, &c.maxy);
		projection->project(c.lon2, c.lat2, 32, &c.maxx, &c.miny);
	}

	if (max_tilestats_sample_values < max_tilestats_values) {
		max_tilestats_sample_values = max_tilestats_values;
	}

	signal(SIGPIPE, SIG_IGN);

	files_open_at_start = open("/dev/null", O_RDONLY | O_CLOEXEC);
	if (files_open_at_start < 0) {
		perror("open /dev/null");
		exit(EXIT_FAILURE);
	}
	if (close(files_open_at_start) != 0) {
		perror("close");
		exit(EXIT_FAILURE);
	}

	if (full_detail <= 0) {
		full_detail = 12;
	}

	if (full_detail < min_detail) {
		min_detail = full_detail;
		fprintf(stderr, "%s: Reducing minimum detail to match full detail %d\n", argv[0], min_detail);
	}

	if (low_detail < min_detail) {
		min_detail = low_detail;
		fprintf(stderr, "%s: Reducing minimum detail to match low detail %d\n", argv[0], min_detail);
	}

	// Need two checks: one for geometry representation, the other for
	// index traversal when guessing base zoom and drop rate
	if (!guess_maxzoom) {
		if (maxzoom > 32 - full_detail) {
			maxzoom = 32 - full_detail;
			fprintf(stderr, "Highest supported zoom with detail %d is %d\n", full_detail, maxzoom);
		}
		if (maxzoom > 33 - low_detail) {  // that is, maxzoom - 1 > 32 - low_detail
			maxzoom = 33 - low_detail;
			fprintf(stderr, "Highest supported zoom with low detail %d is %d\n", low_detail, maxzoom);
		}
	}
	if (maxzoom > MAX_ZOOM) {
		maxzoom = MAX_ZOOM;
		fprintf(stderr, "Highest supported zoom is %d\n", maxzoom);
	}

	if (minzoom > maxzoom) {
		fprintf(stderr, "%s: Minimum zoom -Z%d cannot be greater than maxzoom -z%d\n", argv[0], minzoom, maxzoom);
		exit(EXIT_FAILURE);
	}

	if (basezoom == -1) {
		if (!guess_maxzoom) {
			basezoom = maxzoom;
		}
	}

	geometry_scale = 32 - (full_detail + maxzoom);
	if (geometry_scale < 0) {
		geometry_scale = 0;
		if (!guess_maxzoom) {
			fprintf(stderr, "Full detail + maxzoom > 32, so you are asking for more detail than is available.\n");
		}
	}

	if ((basezoom < 0 || droprate < 0) && (gamma < 0)) {
		// Can't use randomized (as opposed to evenly distributed) dot dropping
		// if rate and base aren't known during feature reading.
		gamma = 0;
		fprintf(stderr, "Forcing -g0 since -B or -r is not known\n");
	}

	if (out_mbtiles == NULL && out_dir == NULL) {
		fprintf(stderr, "%s: must specify -o out.mbtiles or -e directory\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (out_mbtiles != NULL && out_dir != NULL) {
		fprintf(stderr, "%s: Options -o and -e cannot be used together\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (out_mbtiles != NULL) {
		if (force) {
			unlink(out_mbtiles);
		}

		outdb = mbtiles_open(out_mbtiles, argv, forcetable);
	}
	if (out_dir != NULL) {
		check_dir(out_dir, argv, force, forcetable);
	}

	int ret = EXIT_SUCCESS;

	for (i = optind; i < argc; i++) {
		struct source src;
		src.layer = "";
		src.file = std::string(argv[i]);
		sources.push_back(src);
	}

	if (sources.size() == 0) {
		struct source src;
		src.layer = "";
		src.file = "";  // standard input
		sources.push_back(src);
	}

	if (layername != NULL) {
		for (size_t a = 0; a < sources.size(); a++) {
			sources[a].layer = layername;
		}
	}

	long long file_bbox[4] = {UINT_MAX, UINT_MAX, 0, 0};

	ret = read_input(sources, name ? name : out_mbtiles ? out_mbtiles : out_dir, maxzoom, minzoom, basezoom, basezoom_marker_width, outdb, out_dir, &exclude, &include, exclude_all, filter, droprate, buffer, tmpdir, gamma, read_parallel, forcetable, attribution, gamma != 0, file_bbox, prefilter, postfilter, description, guess_maxzoom, &attribute_types, argv[0], &attribute_accum, attribute_descriptions, commandline);

	if (outdb != NULL) {
		mbtiles_close(outdb, argv[0]);
	}

#ifdef MTRACE
	muntrace();
#endif

	i = open("/dev/null", O_RDONLY | O_CLOEXEC);
	// i < files_open_at_start is not an error, because reading from a pipe closes stdin
	if (i > files_open_at_start) {
		fprintf(stderr, "Internal error: did not close all files: %d\n", i);
		exit(EXIT_FAILURE);
	}

	if (filter != NULL) {
		json_free(filter);
	}

	return ret;
}

int mkstemp_cloexec(char *name) {
	int fd = mkstemp(name);
	if (fd >= 0) {
		if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
			perror("cloexec for temporary file");
			exit(EXIT_FAILURE);
		}
	}
	return fd;
}

FILE *fopen_oflag(const char *name, const char *mode, int oflag) {
	int fd = open(name, oflag);
	if (fd < 0) {
		return NULL;
	}
	return fdopen(fd, mode);
}

bool progress_time() {
	if (progress_interval == 0.0) {
		return true;
	}

	struct timeval tv;
	double now;
	if (gettimeofday(&tv, NULL) != 0) {
		fprintf(stderr, "%s: Can't get the time of day: %s\n", *av, strerror(errno));
		now = 0;
	} else {
		now = tv.tv_sec + tv.tv_usec / 1000000.0;
	}

	if (now - last_progress >= progress_interval) {
		last_progress = now;
		return true;
	} else {
		return false;
	}
}
