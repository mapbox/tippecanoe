#include <string>
#include <vector>

#ifndef DIRTILES_HPP
#define DIRTILES_HPP

void dir_write_tile(const char *outdir, int z, int tx, int ty, std::string const &pbf);

void check_dir(const char *d, bool force, bool forcetable);

struct zxy {
	long long z;
	long long x;
	long long y;

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
		return std::to_string(z) + "/" + std::to_string(x) + "/" + std::to_string(y) + ".pbf";
	}
};

std::vector<zxy> enumerate_dirtiles(const char *fname);
sqlite3 *dirmeta2tmp(const char *fname);
std::string dir_read_tile(std::string pbfPath, struct zxy tile);

#endif
