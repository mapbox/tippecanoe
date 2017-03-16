struct type_and_string {
	int type;
	std::string string;

	bool operator<(const type_and_string &o) const;
};

struct layermap_entry {
	size_t id;
	std::set<type_and_string> file_keys;
	int minzoom;
	int maxzoom;

	layermap_entry(size_t _id) {
		id = _id;
	}
};

sqlite3 *mbtiles_open(char *dbname, char **argv, int forcetable);

void mbtiles_write_tile(sqlite3 *outdb, int z, int tx, int ty, const char *data, int size);

void mbtiles_write_metadata(sqlite3 *outdb, const char *fname, int minzoom, int maxzoom, double minlat, double minlon, double maxlat, double maxlon, double midlat, double midlon, int forcetable, const char *attribution, std::map<std::string, layermap_entry> const &layermap, bool vector);

void mbtiles_close(sqlite3 *outdb, char **argv);

void aprintf(std::string *buf, const char *format, ...);

std::map<std::string, layermap_entry> merge_layermaps(std::vector<std::map<std::string, layermap_entry> > const &maps);
