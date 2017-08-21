#ifndef MBTILES_HPP
#define MBTILES_HPP

#include <math.h>
#include <map>
#include "mvt.hpp"

struct type_and_string {
	int type;
	std::string string;

	bool operator<(const type_and_string &o) const;
	bool operator!=(const type_and_string &o) const;
};

struct type_and_string_stats {
	std::vector<type_and_string> sample_values;  // sorted
	double min = INFINITY;
	double max = -INFINITY;
	int type = 0;
};

struct layermap_entry {
	size_t id;
	std::map<std::string, type_and_string_stats> file_keys;
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

void mbtiles_write_metadata(sqlite3 *outdb, const char *outdir, const char *fname, int minzoom, int maxzoom, double minlat, double minlon, double maxlat, double maxlon, double midlat, double midlon, int forcetable, const char *attribution, std::map<std::string, layermap_entry> const &layermap, bool vector, const char *description, bool do_tilestats);

void mbtiles_close(sqlite3 *outdb, const char *pgm);

void aprintf(std::string *buf, const char *format, ...);

std::map<std::string, layermap_entry> merge_layermaps(std::vector<std::map<std::string, layermap_entry> > const &maps);
std::map<std::string, layermap_entry> merge_layermaps(std::vector<std::map<std::string, layermap_entry> > const &maps, bool trunc);

void add_to_file_keys(std::map<std::string, type_and_string_stats> &file_keys, std::string const &layername, type_and_string const &val);

#endif
