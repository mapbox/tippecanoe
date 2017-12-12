#ifndef TILE_HPP
#define TILE_HPP

#include <stdio.h>
#include <sqlite3.h>
#include <vector>
#include <map>
#include "mbtiles.hpp"

long write_tile(char **geom, char *metabase, char *stringpool, unsigned *file_bbox, int z, unsigned x, unsigned y, int detail, int min_detail, int basezoom, sqlite3 *outdb, const char *outdir, double droprate, long buffer, const char *fname, FILE **geomfile, int file_minzoom, int file_maxzoom, double todo, char *geomstart, long along, double gamma, size_t nlayers);

int traverse_zooms(int *geomfd, off_t *geom_size, char *metabase, char *stringpool, unsigned *midx, unsigned *midy, int &maxzoom, int minzoom, sqlite3 *outdb, const char *outdir, long buffer, const char *fname, const char *tmpdir, double gamma, int full_detail, int low_detail, int min_detail, long *meta_off, long *pool_off, unsigned *initial_x, unsigned *initial_y, double simplification, std::vector<std::map<std::string, layermap_entry> > &layermap, const char *prefilter, const char *postfilter);

bool manage_gap(unsigned long index, unsigned long *previndex, double scale, double gamma, double *gap);

#endif
