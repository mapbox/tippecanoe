#ifndef GEOBUF_HPP
#define GEOBUF_HPP

#include <stdio.h>
#include <set>
#include <map>
#include <string>
#include "mbtiles.hpp"
#include "serial.hpp"

void parse_geobuf(std::vector<struct serialization_state> *sst, const char *s, size_t len, int layer, std::string layername);

#endif
