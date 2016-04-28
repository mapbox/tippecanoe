long long write_tile(char **geom, char *metabase, char *stringpool, unsigned *file_bbox, int z, unsigned x, unsigned y, int detail, int min_detail, int basezoom, char **layernames, sqlite3 *outdb, double droprate, int buffer, const char *fname, FILE **geomfile, int file_minzoom, int file_maxzoom, double todo, char *geomstart, long long along, double gamma, int nlayers, int *prevent, int *additional);

int traverse_zooms(int *geomfd, off_t *geom_size, char *metabase, char *stringpool, unsigned *midx, unsigned *midy, char **layernames, int maxzoom, int minzoom, int basezoom, sqlite3 *outdb, double droprate, int buffer, const char *fname, const char *tmpdir, double gamma, int nlayers, int *prevent, int *additional, int full_detail, int low_detail, int min_detail, long long *meta_off, long long *pool_off, unsigned *initial_x, unsigned *initial_y);

int manage_gap(unsigned long long index, unsigned long long *previndex, double scale, double gamma, double *gap);
