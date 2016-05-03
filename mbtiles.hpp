struct type_and_string {
	int type;
	std::string string;

	bool operator<(const type_and_string &o) const;
};

sqlite3 *mbtiles_open(char *dbname, char **argv, int forcetable);

void mbtiles_write_tile(sqlite3 *outdb, int z, int tx, int ty, const char *data, int size);

void mbtiles_write_metadata(sqlite3 *outdb, const char *fname, std::vector<std::string> &layername, int minzoom, int maxzoom, double minlat, double minlon, double maxlat, double maxlon, double midlat, double midlon, std::vector<std::set<type_and_string> > &file_keys, int nlayers, int forcetable, const char *attribution);

void mbtiles_close(sqlite3 *outdb, char **argv);
