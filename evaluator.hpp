#ifndef EVALUATOR_HPP
#define EVALUATOR HPP

#include <map>
#include <string>
#include "jsonpull/jsonpull.hpp"
#include "mvt.hpp"

bool evaluate(std::map<std::string, mvt_value> const &feature, std::string const &layer, std::shared_ptr<json_object> filter);
std::shared_ptr<json_object> parse_filter(const char *s);
std::shared_ptr<json_object> read_filter(const char *fname);

#endif
