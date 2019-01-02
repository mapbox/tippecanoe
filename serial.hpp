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

void serialize_int(FILE *out, int32_t n, std::atomic<int64_t> *fpos, const char *fname);
void serialize_long_long(FILE *out, int64_t n, std::atomic<int64_t> *fpos, const char *fname);
void serialize_ulong_long(FILE *out, uint64_t n, std::atomic<int64_t> *fpos, const char *fname);
void serialize_byte(FILE *out, signed char n, std::atomic<int64_t> *fpos, const char *fname);
void serialize_uint(FILE *out, uint32_t n, std::atomic<int64_t> *fpos, const char *fname);
void serialize_string(FILE *out, const char *s, std::atomic<int64_t> *fpos, const char *fname);

void deserialize_int(char **f, int32_t *n);
void deserialize_long_long(char **f, int64_t *n);
void deserialize_ulong_long(char **f, uint64_t *n);
void deserialize_uint(char **f, uint32_t *n);
void deserialize_byte(char **f, signed char *n);

int32_t deserialize_int_io(FILE *f, int32_t *n, std::atomic<int64_t> *geompos);
int32_t deserialize_long_long_io(FILE *f, int64_t *n, std::atomic<int64_t> *geompos);
int32_t deserialize_ulong_long_io(FILE *f, uint64_t *n, std::atomic<int64_t> *geompos);
int32_t deserialize_uint_io(FILE *f, uint32_t *n, std::atomic<int64_t> *geompos);
int32_t deserialize_byte_io(FILE *f, signed char *n, std::atomic<int64_t> *geompos);

struct serial_val {
	int32_t type = 0;
	std::string s = "";
};

struct serial_feature {
	int64_t layer = 0;
	int32_t segment = 0;
	int64_t seq = 0;

	signed char t = 0;
	signed char feature_minzoom = 0;

	bool has_id = false;
	uint64_t id = 0;

	bool has_tippecanoe_minzoom = false;
	int32_t tippecanoe_minzoom = 0;

	bool has_tippecanoe_maxzoom = false;
	int32_t tippecanoe_maxzoom = 0;

	drawvec geometry = drawvec();
	uint64_t index = 0;
	int64_t extent = 0;

	std::vector<int64_t> keys{};
	std::vector<int64_t> values{};
	// If >= 0, metadata is external
	int64_t metapos = 0;

	// XXX This isn't serialized. Should it be here?
	int64_t bbox[4] = {0, 0, 0, 0};
	std::vector<std::string> full_keys{};
	std::vector<serial_val> full_values{};
	std::string layername = "";
	bool dropped = false;
};

void serialize_feature(FILE *geomfile, serial_feature *sf, std::atomic<int64_t> *geompos, const char *fname, int64_t wx, int64_t wy, bool include_minzoom);
serial_feature deserialize_feature(FILE *geoms, std::atomic<int64_t> *geompos_in, char *metabase, int64_t *meta_off, uint32_t z, uint32_t tx, uint32_t ty, uint32_t *initial_x, uint32_t *initial_y);

struct reader {
	int32_t metafd = -1;
	int32_t poolfd = -1;
	int32_t treefd = -1;
	int32_t geomfd = -1;
	int32_t indexfd = -1;

	FILE *metafile = NULL;
	struct memfile *poolfile = NULL;
	struct memfile *treefile = NULL;
	FILE *geomfile = NULL;
	FILE *indexfile = NULL;

	std::atomic<int64_t> metapos;
	std::atomic<int64_t> geompos;
	std::atomic<int64_t> indexpos;

	int64_t file_bbox[4] = {0, 0, 0, 0};

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

		int64_t p = r.metapos;
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
	int32_t line = 0;	  // user-oriented location within source for error reports

	std::atomic<int64_t> *layer_seq = NULL;     // sequence within current layer
	std::atomic<int64_t> *progress_seq = NULL;  // overall sequence for progress indicator

	std::vector<struct reader> *readers = NULL;  // array of data for each input thread
	int32_t segment = 0;			     // the current input thread

	uint32_t *initial_x = NULL;  // relative offset of all geometries
	uint32_t *initial_y = NULL;
	int32_t *initialized = NULL;

	double *dist_sum = NULL;  // running tally for calculation of resolution within features
	size_t *dist_count = NULL;
	bool want_dist = false;

	int32_t maxzoom = 0;
	int32_t basezoom = 0;

	bool filters = false;
	bool uses_gamma = false;

	std::map<std::string, layermap_entry> *layermap = NULL;

	std::map<std::string, int32_t> const *attribute_types = NULL;
	std::set<std::string> *exclude = NULL;
	std::set<std::string> *include = NULL;
	int32_t exclude_all = 0;
	json_object *filter = NULL;
};

int32_t serialize_feature(struct serialization_state *sst, serial_feature &sf);
void coerce_value(std::string const &key, int32_t &vt, std::string &val, std::map<std::string, int32_t> const *attribute_types);

#endif
