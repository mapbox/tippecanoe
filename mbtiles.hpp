#ifndef MBTILES_HPP
#define MBTILES_HPP

#include <math.h>
#include <map>
#include "mvt.hpp"

extern size_t max_tilestats_attributes;
extern size_t max_tilestats_sample_values;
extern size_t max_tilestats_values;

struct type_and_string {
	int type = 0;
	std::string string = "";

	bool operator<(const type_and_string &o) const;
	bool operator!=(const type_and_string &o) const;
};

struct type_and_string_stats {
	std::vector<type_and_string> sample_values = std::vector<type_and_string>();  // sorted
	double min = INFINITY;
	double max = -INFINITY;
	int type = 0;
};

struct layermap_entry {
	size_t id = 0;
	std::map<std::string, type_and_string_stats> file_keys{};
	int minzoom = 0;
	int maxzoom = 0;
	std::string description = "";

	size_t points = 0;
	size_t lines = 0;
	size_t polygons = 0;
	size_t retain = 0;  // keep for tilestats, even if no features directly here

	layermap_entry(size_t _id) {
		id = _id;
	}
};

sqlite3 *mbtiles_open(char *dbname, char **argv, int forcetable);

void mbtiles_write_tile(sqlite3 *outdb, int z, int tx, int ty, const char *data, int size);

void mbtiles_write_metadata(sqlite3 *outdb, const char *outdir, const char *fname, int minzoom, int maxzoom, double minlat, double minlon, double maxlat, double maxlon, double midlat, double midlon, int forcetable, const char *attribution, std::map<std::string, layermap_entry> const &layermap, bool vector, const char *description, bool do_tilestats, std::map<std::string, std::string> const &attribute_descriptions, std::string const &program, std::string const &commandline);

void mbtiles_close(sqlite3 *outdb, const char *pgm);

std::map<std::string, layermap_entry> merge_layermaps(std::vector<std::map<std::string, layermap_entry> > const &maps);
std::map<std::string, layermap_entry> merge_layermaps(std::vector<std::map<std::string, layermap_entry> > const &maps, bool trunc);

void add_to_file_keys(std::map<std::string, type_and_string_stats> &file_keys, std::string const &layername, type_and_string const &val);

#endif
