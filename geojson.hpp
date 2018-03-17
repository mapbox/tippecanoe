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
	json_pull *jp;
	int layer;
	std::string *layername;

	struct serialization_state *sst;

	parse_json_args(json_pull *jp1, int layer1, std::string *layername1, struct serialization_state *sst1)
	    : jp(jp1), layer(layer1), layername(layername1), sst(sst1) {
	}
};

struct json_pull *json_begin_map(char *map, long long len);
void json_end_map(struct json_pull *jp);

void parse_json(struct serialization_state *sst, json_pull *jp, int layer, std::string layername);
void *run_parse_json(void *v);

#endif
