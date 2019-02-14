#ifndef SERIAL_HPP
#define SERIAL_HPP

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <atomic>
#include <sys/stat.h>
#include "geometry.hpp"
#include "mbtiles.hpp"
#include "jsonpull/jsonpull.h"

size_t fwrite_check(const void *ptr, size_t size, size_t nitems, FILE *stream, const char *fname);

void serialize_int(FILE *out, int n, std::atomic<long long> *fpos, const char *fname);
void serialize_long_long(FILE *out, long long n, std::atomic<long long> *fpos, const char *fname);
void serialize_ulong_long(FILE *out, unsigned long long n, std::atomic<long long> *fpos, const char *fname);
void serialize_byte(FILE *out, signed char n, std::atomic<long long> *fpos, const char *fname);
void serialize_uint(FILE *out, unsigned n, std::atomic<long long> *fpos, const char *fname);
void serialize_string(FILE *out, const char *s, std::atomic<long long> *fpos, const char *fname);

void deserialize_int(char **f, int *n);
void deserialize_long_long(char **f, long long *n);
void deserialize_ulong_long(char **f, unsigned long long *n);
void deserialize_uint(char **f, unsigned *n);
void deserialize_byte(char **f, signed char *n);

int deserialize_int_io(FILE *f, int *n, std::atomic<long long> *geompos);
int deserialize_long_long_io(FILE *f, long long *n, std::atomic<long long> *geompos);
int deserialize_ulong_long_io(FILE *f, unsigned long long *n, std::atomic<long long> *geompos);
int deserialize_uint_io(FILE *f, unsigned *n, std::atomic<long long> *geompos);
int deserialize_byte_io(FILE *f, signed char *n, std::atomic<long long> *geompos);

struct serial_val {
	int type = 0;
	std::string s = "";
};

struct serial_feature {
	long long layer = 0;
	int segment = 0;
	long long seq = 0;

	signed char t = 0;
	signed char feature_minzoom = 0;

	bool has_id = false;
	unsigned long long id = 0;

	bool has_tippecanoe_minzoom = false;
	int tippecanoe_minzoom = 0;

	bool has_tippecanoe_maxzoom = false;
	int tippecanoe_maxzoom = 0;

	drawvec geometry = drawvec();
	unsigned long long index = 0;
	long long extent = 0;

	std::vector<long long> keys{};
	std::vector<long long> values{};
	// If >= 0, metadata is external
	long long metapos = 0;

	// XXX This isn't serialized. Should it be here?
	long long bbox[4] = {0, 0, 0, 0};
	std::vector<std::string> full_keys{};
	std::vector<serial_val> full_values{};
	std::string layername = "";
	bool dropped = false;
};

void serialize_feature(FILE *geomfile, serial_feature *sf, std::atomic<long long> *geompos, const char *fname, long long wx, long long wy, bool include_minzoom);
serial_feature deserialize_feature(FILE *geoms, std::atomic<long long> *geompos_in, char *metabase, long long *meta_off, unsigned z, unsigned tx, unsigned ty, unsigned *initial_x, unsigned *initial_y);

struct reader {
	int metafd = -1;
	int poolfd = -1;
	int treefd = -1;
	int geomfd = -1;
	int indexfd = -1;

	FILE *metafile = NULL;
	struct memfile *poolfile = NULL;
	struct memfile *treefile = NULL;
	FILE *geomfile = NULL;
	FILE *indexfile = NULL;

	std::atomic<long long> metapos;
	std::atomic<long long> geompos;
	std::atomic<long long> indexpos;

	long long file_bbox[4] = {0, 0, 0, 0};

	struct stat geomst {};
	struct stat metast {};

	char *geom_map = NULL;

	reader()
	    : metapos(0), geompos(0), indexpos(0) {
	}

	reader(reader const &r) {
		metafd = r.metafd;
		poolfd = r.poolfd;
		treefd = r.treefd;
		geomfd = r.geomfd;
		indexfd = r.indexfd;

		metafile = r.metafile;
		poolfile = r.poolfile;
		treefile = r.treefile;
		geomfile = r.geomfile;
		indexfile = r.indexfile;

		long long p = r.metapos;
		metapos = p;

		p = r.geompos;
		geompos = p;

		p = r.indexpos;
		indexpos = p;

		memcpy(file_bbox, r.file_bbox, sizeof(file_bbox));

		geomst = r.geomst;
		metast = r.metast;

		geom_map = r.geom_map;
	}
};

struct serialization_state {
	const char *fname = NULL;  // source file name
	int line = 0;		   // user-oriented location within source for error reports

	std::atomic<long long> *layer_seq = NULL;     // sequence within current layer
	std::atomic<long long> *progress_seq = NULL;  // overall sequence for progress indicator

	std::vector<struct reader> *readers = NULL;  // array of data for each input thread
	int segment = 0;			     // the current input thread

	unsigned *initial_x = NULL;  // relative offset of all geometries
	unsigned *initial_y = NULL;
	int *initialized = NULL;

	double *dist_sum = NULL;  // running tally for calculation of resolution within features
	size_t *dist_count = NULL;
	bool want_dist = false;

	int maxzoom = 0;
	int basezoom = 0;

	bool filters = false;
	bool uses_gamma = false;

	std::map<std::string, layermap_entry> *layermap = NULL;

	std::map<std::string, int> const *attribute_types = NULL;
	std::set<std::string> *exclude = NULL;
	std::set<std::string> *include = NULL;
	int exclude_all = 0;
};

int serialize_feature(struct serialization_state *sst, serial_feature &sf);
void coerce_value(std::string const &key, int &vt, std::string &val, std::map<std::string, int> const *attribute_types);

#endif
