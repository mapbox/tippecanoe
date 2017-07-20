#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "dirtiles.hpp"

std::string dir_read_tile(std::string pbfPath) {
	std::ifstream pbfFile(pbfPath, std::ios::in | std::ios::binary);
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

bool numeric(const char *s) {
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

bool pbfname(const char *s) {
	while (*s >= '0' && *s <= '9') {
		s++;
	}

	return strcmp(s, ".pbf") == 0;
}

void check_dir(const char *dir, bool rm) {
	struct stat st;

	std::string meta = std::string(dir) + "/" + "metadata.json";
	if (rm) {
		unlink(meta.c_str());  // error OK since it may not exist;
	} else {
		if (stat(meta.c_str(), &st) == 0) {
			fprintf(stderr, "%s: file exists\n", meta.c_str());
			exit(EXIT_FAILURE);
		}
	}

	DIR *d1 = opendir(dir);
	if (d1 != NULL) {
		struct dirent *dp;
		while ((dp = readdir(d1)) != NULL) {
			if (numeric(dp->d_name)) {
				std::string z = std::string(dir) + "/" + dp->d_name;

				DIR *d2 = opendir(z.c_str());
				if (d2 == NULL) {
					perror(z.c_str());
					exit(EXIT_FAILURE);
				}

				struct dirent *dp2;
				while ((dp2 = readdir(d2)) != NULL) {
					if (numeric(dp2->d_name)) {
						std::string x = z + "/" + dp2->d_name;

						DIR *d3 = opendir(x.c_str());
						if (d3 == NULL) {
							perror(x.c_str());
							exit(EXIT_FAILURE);
						}

						struct dirent *dp3;
						while ((dp3 = readdir(d3)) != NULL) {
							if (pbfname(dp3->d_name)) {
								std::string y = x + "/" + dp3->d_name;

								if (rm) {
									if (unlink(y.c_str()) != 0) {
										perror(y.c_str());
										exit(EXIT_FAILURE);
									}
								} else {
									fprintf(stderr, "%s: file exists\n", y.c_str());
									exit(EXIT_FAILURE);
								}
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
}
