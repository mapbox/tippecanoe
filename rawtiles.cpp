#include <iostream>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include "rawtiles.hpp"

void write_raw_tile(char *outdir, int z, int tx, int ty, std::string pbf) {
	mkdir(outdir, S_IRWXU | S_IRWXG | S_IRWXO);
	std::string curdir(outdir);
	std::string slash( "/" );
	std::string newdir = curdir + slash + std::to_string(z);
	mkdir(newdir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
	newdir = newdir + "/" + std::to_string(tx);
	mkdir(newdir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
	newdir = newdir + "/" + std::to_string(ty) + ".pbf";

	std::ofstream pbfFile (newdir, std::ios::out | std::ios::binary);
	pbfFile.write (pbf.data(), pbf.size());
	pbfFile.close();
}