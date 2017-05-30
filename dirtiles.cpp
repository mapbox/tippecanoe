#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
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

	std::ofstream pbfFile(newdir, std::ios::out | std::ios::binary);
	pbfFile.write(pbf.data(), pbf.size());
	pbfFile.close();
}
