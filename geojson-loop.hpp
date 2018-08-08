#include <string>
#include "jsonpull/jsonpull.h"

struct json_feature_action {
	std::string fname;

	virtual int add_feature(json_object *geometry, bool geometrycollection, json_object *properties, json_object *id, json_object *tippecanoe, json_object *feature) = 0;
	virtual void check_crs(json_object *j) = 0;
};

void parse_json(json_feature_action *action, json_pull *jp);
