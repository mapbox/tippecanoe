#ifndef TILE_HPP
#define TILE_HPP

#include <stdio.h>
#include <sqlite3.h>
#include <vector>
#include <atomic>
#include <map>
#include "mbtiles.hpp"
#include "jsonpull/jsonpull.h"

enum attribute_op {
	op_sum,
	op_product,
	op_mean,
	op_concat,
	op_comma,
	op_max,
	op_min,
};

long long write_tile(char **geom, char *metabase, char *stringpool, unsigned *file_bbox, int z, unsigned x, unsigned y, int detail, int min_detail, int basezoom, sqlite3 *outdb, const char *outdir, double droprate, int buffer, const char *fname, FILE **geomfile, int file_minzoom, int file_maxzoom, double todo, char *geomstart, long long along, double gamma, int nlayers);

int traverse_zooms(int *geomfd, off_t *geom_size, char *metabase, char *stringpool, std::atomic<unsigned> *midx, std::atomic<unsigned> *midy, int &maxzoom, int minzoom, sqlite3 *outdb, const char *outdir, int buffer, const char *fname, const char *tmpdir, double gamma, int full_detail, int low_detail, int min_detail, long long *meta_off, long long *pool_off, unsigned *initial_x, unsigned *initial_y, double simplification, std::vector<std::map<std::string, layermap_entry> > &layermap, const char *prefilter, const char *postfilter, std::map<std::string, attribute_op> const *attribute_accum, struct json_object *filter);

int manage_gap(unsigned long long index, unsigned long long *previndex, double scale, double gamma, double *gap);

#endif
