#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "dirtiles.hpp"

std::string dir_read_tile(std::string base, struct zxy tile) {
	std::ifstream pbfFile(base + "/" + tile.path(), std::ios::in | std::ios::binary);
	std::ostringstream contents;
	contents << pbfFile.rdbuf();
	pbfFile.close();

	return (contents.str());
}

void dir_write_tile(const char *outdir, int z, int tx, int ty, std::string const &pbf) {
	mkdir(outdir, S_IRWXU | S_IRWXG | S_IRWXO);
	std::string curdir(outdir);
	std::string slash("/");
	std::string newdir = curdir + slash + std::to_string(z);
	mkdir(newdir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
	newdir = newdir + "/" + std::to_string(tx);
	mkdir(newdir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
	newdir = newdir + "/" + std::to_string(ty) + ".pbf";

	struct stat st;
	if (stat(newdir.c_str(), &st) == 0) {
		fprintf(stderr, "Can't write tile to already existing %s\n", newdir.c_str());
		exit(EXIT_FAILURE);
	}

	std::ofstream pbfFile(newdir, std::ios::out | std::ios::binary);
	pbfFile.write(pbf.data(), pbf.size());
	pbfFile.close();
}

static bool numeric(const char *s) {
	if (*s == '\0') {
		return false;
	}
	for (; *s != 0; s++) {
		if (*s < '0' || *s > '9') {
			return false;
		}
	}
	return true;
}

static bool pbfname(const char *s) {
	while (*s >= '0' && *s <= '9') {
		s++;
	}

	return strcmp(s, ".pbf") == 0;
}

void check_dir(const char *dir, bool force, bool forcetable) {
	struct stat st;

	std::string meta = std::string(dir) + "/" + "metadata.json";
	if (force) {
		unlink(meta.c_str());  // error OK since it may not exist;
	} else {
		if (stat(meta.c_str(), &st) == 0) {
			fprintf(stderr, "%s: file exists\n", meta.c_str());
			if (!forcetable) {
				exit(EXIT_FAILURE);
			}
		}
	}

	if (forcetable) {
		// Don't clear existing tiles
		return;
	}

	std::vector<zxy> tiles = enumerate_dirtiles(dir);

	for (size_t i = 0; i < tiles.size(); i++) {
		std::string fn = std::string(dir) + "/" + tiles[i].path();

		if (force) {
			if (unlink(fn.c_str()) != 0) {
				perror(fn.c_str());
				exit(EXIT_FAILURE);
			}
		} else {
			fprintf(stderr, "%s: file exists\n", fn.c_str());
			exit(EXIT_FAILURE);
		}
	}
}

std::vector<zxy> enumerate_dirtiles(const char *fname) {
	std::vector<zxy> tiles;

	DIR *d1 = opendir(fname);
	if (d1 != NULL) {
		struct dirent *dp;
		while ((dp = readdir(d1)) != NULL) {
			if (numeric(dp->d_name)) {
				std::string z = std::string(fname) + "/" + dp->d_name;
				int tz = atoi(dp->d_name);

				DIR *d2 = opendir(z.c_str());
				if (d2 == NULL) {
					perror(z.c_str());
					exit(EXIT_FAILURE);
				}

				struct dirent *dp2;
				while ((dp2 = readdir(d2)) != NULL) {
					if (numeric(dp2->d_name)) {
						std::string x = z + "/" + dp2->d_name;
						int tx = atoi(dp2->d_name);

						DIR *d3 = opendir(x.c_str());
						if (d3 == NULL) {
							perror(x.c_str());
							exit(EXIT_FAILURE);
						}

						struct dirent *dp3;
						while ((dp3 = readdir(d3)) != NULL) {
							if (pbfname(dp3->d_name)) {
								int ty = atoi(dp3->d_name);
								tiles.push_back(zxy(tz, tx, ty));
							}
						}

						closedir(d3);
					}
				}

				closedir(d2);
			}
		}

		closedir(d1);
	}

	std::sort(tiles.begin(), tiles.end());
	return tiles;
}
