#ifndef GEOBUF_HPP
#define GEOBUF_HPP

#include <stdio.h>
#include <set>
#include <map>
#include <string>
#include "mbtiles.hpp"

void parse_geobuf(FILE *fp, const char *reading, volatile long long *layer_seq, volatile long long *progress_seq, long long *metapos, long long *geompos, long long *indexpos, std::set<std::string> *exclude, std::set<std::string> *include, int exclude_all, FILE *metafile, FILE *geomfile, FILE *indexfile, struct memfile *poolfile, struct memfile *treefile, char *fname, int basezoom, int layer, double droprate, long long *file_bbox, int segment, int *initialized, unsigned *initial_x, unsigned *initial_y, struct reader *readers, int maxzoom, std::map<std::string, layermap_entry> *layermap, std::string layername, bool uses_gamma, std::map<std::string, int> const *attribute_types, double *dist_sum, size_t *dist_count, bool want_dist);
void *run_parse_json(void *v);

#endif
