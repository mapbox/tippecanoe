#ifndef GEOJSON_HPP
#define GEOJSON_HPP

#include <stdio.h>
#include <set>
#include <map>
#include <string>
#include "mbtiles.hpp"
#include "jsonpull/jsonpull.h"
#include "serial.hpp"

struct parse_json_args {
	json_pull *jp = NULL;
	int layer = 0;
	std::string *layername = NULL;
	std::map<std::string, int> const *attribute_types = NULL;
	bool want_dist = false;

	struct serialization_state *sst = NULL;
};

struct json_pull *json_begin_map(char *map, long long len);
void json_end_map(struct json_pull *jp);

void parse_json(struct serialization_state *sst, json_pull *jp, int layer, std::string layername);
void *run_parse_json(void *v);

#endif
