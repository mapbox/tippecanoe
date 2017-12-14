#ifndef SERIAL_HPP
#define SERIAL_HPP

#include <stddef.h>
#include <stdio.h>
#include <vector>
#include <sys/stat.h>
#include "geometry.hpp"
#include "mbtiles.hpp"
#include "jsonpull/jsonpull.h"

size_t fwrite_check(const void *ptr, size_t size, size_t nitems, FILE *stream, const char *fname);

void serialize_int(FILE *out, int n, off_t *fpos, const char *fname);
void serialize_long(FILE *out, long n, off_t *fpos, const char *fname);
void serialize_ulong(FILE *out, unsigned long n, off_t *fpos, const char *fname);
void serialize_byte(FILE *out, signed char n, off_t *fpos, const char *fname);
void serialize_uint(FILE *out, unsigned n, off_t *fpos, const char *fname);
void serialize_string(FILE *out, const char *s, off_t *fpos, const char *fname);

void deserialize_int(char **f, int *n);
void deserialize_long(char **f, long *n);
void deserialize_ulong(char **f, unsigned long *n);
void deserialize_uint(char **f, unsigned *n);
void deserialize_byte(char **f, signed char *n);

bool deserialize_int_io(FILE *f, int *n, off_t *geompos);
bool deserialize_long_io(FILE *f, long *n, off_t *geompos);
bool deserialize_ulong_io(FILE *f, unsigned long *n, off_t *geompos);
bool deserialize_uint_io(FILE *f, unsigned *n, off_t *geompos);
bool deserialize_byte_io(FILE *f, signed char *n, off_t *geompos);

struct serial_val {
	int type = 0;
	std::string s = "";
};

struct serial_feature {
	long layer = 0;
	size_t segment = 0;
	size_t seq = 0;

	signed char t = 0;
	signed char feature_minzoom = 0;

	bool has_id = false;
	unsigned long id = 0;

	bool has_tippecanoe_minzoom = false;
	int tippecanoe_minzoom = 0;

	bool has_tippecanoe_maxzoom = false;
	int tippecanoe_maxzoom = 0;

	drawvec geometry = drawvec();
	unsigned long index = 0;
	long extent = 0;

	size_t m = 0;
	std::vector<long> keys{};
	std::vector<long> values{};
	off_t metapos = 0;

	// XXX This isn't serialized. Should it be here?
	long bbox[4] = {0, 0, 0, 0};
	std::vector<std::string> full_keys{};
	std::vector<serial_val> full_values{};
	std::string layername = "";
};

void serialize_feature(FILE *geomfile, serial_feature *sf, off_t *geompos, const char *fname, long wx, long wy, bool include_minzoom);
serial_feature deserialize_feature(FILE *geoms, off_t *geompos_in, char *metabase, off_t *meta_off, unsigned z, unsigned tx, unsigned ty, long *initial_x, long *initial_y);

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

	off_t metapos = 0;
	off_t geompos = 0;
	off_t indexpos = 0;

	long file_bbox[4] = {0, 0, 0, 0};

	struct stat geomst {};
	struct stat metast {};

	char *geom_map = NULL;
};

struct serialization_state {
	const char *fname = NULL;  // source file name
	size_t line = 0;	   // user-oriented location within source for error reports

	volatile long *layer_seq = NULL;     // sequence within current layer
	volatile long *progress_seq = NULL;  // overall sequence for progress indicator

	std::vector<struct reader> *readers = NULL;  // array of data for each input thread
	size_t segment = 0;			     // the current input thread

	long *initial_x = NULL;  // relative offset of all geometries
	long *initial_y = NULL;
	bool *initialized = NULL;

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
	bool exclude_all = false;
	json_object *filter = NULL;
};

bool serialize_feature(struct serialization_state *sst, serial_feature &sf);
void coerce_value(std::string const &key, int &vt, std::string &val, std::map<std::string, int> const *attribute_types);

#endif
