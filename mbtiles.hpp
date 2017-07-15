#ifndef MBTILES_HPP
#define MBTILES_HPP

#include <math.h>
#include "mvt.hpp"

struct type_and_string {
	int type;
	std::string string;

	size_t attribute_count = 0;
	std::set<mvt_value> sample_values;
	double min = INFINITY;
	double max = -INFINITY;

	bool operator<(const type_and_string &o) const;
};

struct layermap_entry {
	size_t id;
	std::set<type_and_string> file_keys;
	int minzoom;
	int maxzoom;

	size_t points = 0;
	size_t lines = 0;
	size_t polygons = 0;

	layermap_entry(size_t _id) {
		id = _id;
	}
};

sqlite3 *mbtiles_open(char *dbname, char **argv, int forcetable);

void mbtiles_write_tile(sqlite3 *outdb, int z, int tx, int ty, const char *data, int size);

void mbtiles_write_metadata(sqlite3 *outdb, const char *outdir, const char *fname, int minzoom, int maxzoom, double minlat, double minlon, double maxlat, double maxlon, double midlat, double midlon, int forcetable, const char *attribution, std::map<std::string, layermap_entry> const &layermap, bool vector, const char *description);

void mbtiles_close(sqlite3 *outdb, char **argv);

void aprintf(std::string *buf, const char *format, ...);

std::map<std::string, layermap_entry> merge_layermaps(std::vector<std::map<std::string, layermap_entry> > const &maps);

#endif
