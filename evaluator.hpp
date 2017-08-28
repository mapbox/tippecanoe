#ifndef EVALUATOR_HPP
#define EVALUATOR HPP

#include <map>
#include <string>
#include "jsonpull/jsonpull.h"
#include "mvt.hpp"

bool evaluate(std::map<std::string, mvt_value> const &feature, std::string const &layer, json_object *filter);

#endif
