#ifndef EVALUATOR_HPP
#define EVALUATOR HPP

#include <map>
#include <string>
#include <set>
#include "jsonpull/jsonpull.h"
#include "mvt.hpp"

bool evaluate(std::map<std::string, mvt_value> const &feature, std::string const &layer, json_object *filter, std::set<std::string> &exclude_attributes);
json_object *parse_filter(const char *s);
json_object *read_filter(const char *fname);

#endif
