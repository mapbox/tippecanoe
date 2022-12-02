#include <string>
#include <vector>
#include <sys/stat.h>

#ifndef DIRTILES_HPP
#define DIRTILES_HPP

void dir_write_tile(const char *outdir, int z, int tx, int ty, std::string const &pbf);
void dir_erase_zoom(const char *outdir, int z);
void dir_write_metadata(const char *outdir, const metadata &m);

void check_dir(const char *d, char **argv, bool force, bool forcetable);

struct zxy {
	long long z;
	long long x;
	long long y;
	std::string extension = ".pbf";

	zxy(int _z, int _x, int _y)
	    : z(_z), x(_x), y(_y) {
	}

	bool operator<(const zxy &other) const {
		if (z < other.z) {
			return true;
		}
		if (z == other.z) {
			if (x < other.x) {
				return true;
			}
			if (x == other.x) {
				if (y > other.y) {
					return true;  // reversed for TMS
				}
			}
		}

		return false;
	}

	std::string path() {
		return std::to_string(z) + "/" + std::to_string(x) + "/" + std::to_string(y) + extension;
	}
};

std::vector<zxy> enumerate_dirtiles(const char *fname, int minzoom, int maxzoom);
sqlite3 *dirmeta2tmp(const char *fname);
std::string dir_read_tile(std::string pbfPath, struct zxy tile);

#endif
