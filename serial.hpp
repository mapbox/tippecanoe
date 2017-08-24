#ifndef SERIAL_HPP
#define SERIAL_HPP

#include <stddef.h>
#include <stdio.h>
#include <vector>
#include <sys/stat.h>
#include "geometry.hpp"
#include "mbtiles.hpp"

size_t fwrite_check(const void *ptr, size_t size, size_t nitems, FILE *stream, const char *fname);

void serialize_int(FILE *out, int n, long long *fpos, const char *fname);
void serialize_long_long(FILE *out, long long n, long long *fpos, const char *fname);
void serialize_ulong_long(FILE *out, unsigned long long n, long long *fpos, const char *fname);
void serialize_byte(FILE *out, signed char n, long long *fpos, const char *fname);
void serialize_uint(FILE *out, unsigned n, long long *fpos, const char *fname);
void serialize_string(FILE *out, const char *s, long long *fpos, const char *fname);

void deserialize_int(char **f, int *n);
void deserialize_long_long(char **f, long long *n);
void deserialize_ulong_long(char **f, unsigned long long *n);
void deserialize_uint(char **f, unsigned *n);
void deserialize_byte(char **f, signed char *n);

int deserialize_int_io(FILE *f, int *n, long long *geompos);
int deserialize_long_long_io(FILE *f, long long *n, long long *geompos);
int deserialize_ulong_long_io(FILE *f, unsigned long long *n, long long *geompos);
int deserialize_uint_io(FILE *f, unsigned *n, long long *geompos);
int deserialize_byte_io(FILE *f, signed char *n, long long *geompos);

struct serial_val {
	int type;
	std::string s;
};

struct serial_feature {
	long long layer;
	int segment;
	long long seq;

	signed char t;
	signed char feature_minzoom;

	bool has_id;
	unsigned long long id;

	bool has_tippecanoe_minzoom;
	int tippecanoe_minzoom;

	bool has_tippecanoe_maxzoom;
	int tippecanoe_maxzoom;

	drawvec geometry;
	unsigned long long index;
	long long extent;

	size_t m;
	std::vector<long long> keys;
	std::vector<long long> values;
	long long metapos;

	// XXX This isn't serialized. Should it be here?
	long long bbox[4];
	std::vector<std::string> full_keys;
	std::vector<serial_val> full_values;
	std::string layername;
};

void serialize_feature(FILE *geomfile, serial_feature *sf, long long *geompos, const char *fname, long long wx, long long wy, bool include_minzoom);
serial_feature deserialize_feature(FILE *geoms, long long *geompos_in, char *metabase, long long *meta_off, unsigned z, unsigned tx, unsigned ty, unsigned *initial_x, unsigned *initial_y);

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

struct serialization_state {
	const char *fname;  // source file name
	int line;	   // user-oriented location within source for error reports

	volatile long long *layer_seq;     // sequence within current layer
	volatile long long *progress_seq;  // overall sequence for progress indicator

	struct reader *readers;  // array of data for each input thread
	int segment;		 // the current input thread

	unsigned *initial_x;  // relative offset of all geometries
	unsigned *initial_y;
	int *initialized;

	double *dist_sum;  // running tally for calculation of resolution within features
	size_t *dist_count;
	bool want_dist;

	int maxzoom;
	bool filters;
	bool uses_gamma;

	std::map<std::string, layermap_entry> *layermap;
};

#endif
