sqlite3 *mbtiles_open(char *dbname, char **argv);

void mbtiles_write_tile(sqlite3 *outdb, int z, int tx, int ty, const char *data, int size);

void mbtiles_write_metadata(sqlite3 *outdb, char *fname, char *layername, int minzoom, int maxzoom, double minlat, double minlon, double maxlat, double maxlon, double midlat, double midlon, struct pool *fields);

void mbtiles_close(sqlite3 *outdb, char **argv);
