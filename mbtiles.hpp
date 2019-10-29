#ifndef MBTILES_HPP
#define MBTILES_HPP

#include <math.h>
#include <map>
#include "mvt.hpp"

extern size_t max_tilestats_attributes;
extern size_t max_tilestats_sample_values;
extern size_t max_tilestats_values;

struct tilestats_attributes_entry {
	int type = 0;
	std::string string = "";

	bool operator<(const tilestats_attributes_entry &o) const;
	bool operator!=(const tilestats_attributes_entry &o) const;
};

struct tilestats_attributes {
	std::vector<tilestats_attributes_entry> sample_values = std::vector<tilestats_attributes_entry>();  // sorted
	double min = INFINITY;
	double max = -INFINITY;
	int type = 0;
};

struct tilestats_entry {
	size_t id = 0;
	std::map<std::string, tilestats_attributes> tilestats{};
	int minzoom = 0;
	int maxzoom = 0;
	std::string description = "";

	size_t points = 0;
	size_t lines = 0;
	size_t polygons = 0;
	size_t retain = 0;  // keep for tilestats, even if no features directly here

	tilestats_entry(size_t _id) {
		id = _id;
	}
};

sqlite3 *mbtiles_open(char *dbname, char **argv, int forcetable);

void mbtiles_write_tile(sqlite3 *outdb, int z, int tx, int ty, const char *data, int size);

void mbtiles_write_metadata(sqlite3 *outdb, const char *outdir, const char *fname, int minzoom, int maxzoom, double minlat, double minlon, double maxlat, double maxlon, double midlat, double midlon, int forcetable, const char *attribution, std::map<std::string, tilestats_entry> const &tilestat, bool vector, const char *description, bool do_tilestats, std::map<std::string, std::string> const &attribute_descriptions, std::string const &program, std::string const &commandline);

void mbtiles_close(sqlite3 *outdb, const char *pgm);

std::map<std::string, tilestats_entry> merge_layermaps(std::vector<std::map<std::string, tilestats_entry> > const &maps);
std::map<std::string, tilestats_entry> merge_layermaps(std::vector<std::map<std::string, tilestats_entry> > const &maps, bool trunc);

void add_to_tilestats(std::map<std::string, tilestats_attributes> &tilestats, std::string const &layername, tilestats_attributes_entry const &val);

#endif
