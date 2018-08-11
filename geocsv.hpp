#ifndef GEOCSV_HPP
#define GEOCSV_HPP

#include <stdio.h>
#include <set>
#include <map>
#include <string>
#include "mbtiles.hpp"
#include "serial.hpp"

void parse_geocsv(std::vector<struct serialization_state> &sst, std::string fname, int layer, std::string layername);

#endif
