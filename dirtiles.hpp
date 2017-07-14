#include <string>

#ifndef DIRTILES_HPP
#define DIRTILES_HPP

std::string dir_read_tile(std::string pbfPath);

void dir_write_tile(const char *outdir, int z, int tx, int ty, std::string const &pbf);

#endif
