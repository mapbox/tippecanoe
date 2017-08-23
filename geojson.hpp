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
	std::set<std::string> *exclude;
	std::set<std::string> *include;
	int exclude_all;
	int basezoom;
	int layer;
	double droprate;
	int maxzoom;
	std::map<std::string, layermap_entry> *layermap;
	std::string *layername;
	bool uses_gamma;
	std::map<std::string, int> const *attribute_types;
	bool want_dist;
	bool filters;

	struct serialization_state *sst;
};

struct json_pull *json_begin_map(char *map, long long len);
void json_end_map(struct json_pull *jp);

void parse_json(struct serialization_state *sst, json_pull *jp, std::set<std::string> *exclude, std::set<std::string> *include, int exclude_all, int basezoom, int layer, double droprate, int maxzoom, std::map<std::string, layermap_entry> *layermap, std::string layername, bool uses_gamma, std::map<std::string, int> const *attribute_types, bool want_dist, bool filters);
void *run_parse_json(void *v);

#endif
