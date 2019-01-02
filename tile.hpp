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

int64_t write_tile(char **geom, char *metabase, char *stringpool, uint32_t *file_bbox, int32_t z, uint32_t x, uint32_t y, int32_t detail, int32_t min_detail, int32_t basezoom, sqlite3 *outdb, const char *outdir, double droprate, int32_t buffer, const char *fname, FILE **geomfile, int32_t file_minzoom, int32_t file_maxzoom, double todo, char *geomstart, int64_t along, double gamma, int32_t nlayers);

int32_t traverse_zooms(int32_t *geomfd, off_t *geom_size, char *metabase, char *stringpool, std::atomic<uint32_t> *midx, std::atomic<uint32_t> *midy, int32_t &maxzoom, int32_t minzoom, sqlite3 *outdb, const char *outdir, int32_t buffer, const char *fname, const char *tmpdir, double gamma, int32_t full_detail, int32_t low_detail, int32_t min_detail, int64_t *meta_off, int64_t *pool_off, uint32_t *initial_x, uint32_t *initial_y, double simplification, std::vector<std::map<std::string, layermap_entry> > &layermap, const char *prefilter, const char *postfilter, std::map<std::string, attribute_op> const *attribute_accum, struct json_object *filter);

int32_t manage_gap(uint64_t index, uint64_t *previndex, double scale, double gamma, double *gap);

#endif
