#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <string>
#include <map>
#include "jsonpull/jsonpull.h"
#include "geometry.hpp"
#include "projection.hpp"
#include "read_json.hpp"
#include "text.hpp"
#include "mvt.hpp"
#include "milo/dtoa_milo.h"

const char *geometry_names[GEOM_TYPES] = {
	"Point", "MultiPoint", "LineString", "MultiLineString", "Polygon", "MultiPolygon",
};

int geometry_within[GEOM_TYPES] = {
	-1,		 /* point */
	GEOM_POINT,      /* multipoint */
	GEOM_POINT,      /* linestring */
	GEOM_LINESTRING, /* multilinestring */
	GEOM_LINESTRING, /* polygon */
	GEOM_POLYGON,    /* multipolygon */
};

int mb_geometry[GEOM_TYPES] = {
	VT_POINT, VT_POINT, VT_LINE, VT_LINE, VT_POLYGON, VT_POLYGON,
};

void json_context(json_object *j) {
	char *s = json_stringify(j);

	if (strlen(s) >= 500) {
		sprintf(s + 497, "...");
	}

	fprintf(stderr, "In JSON object %s\n", s);
	free(s);  // stringify
}

void parse_geometry(int t, json_object *j, drawvec &out, int op, const char *fname, int line, json_object *feature, bool doing_attributes) {
	int within = geometry_within[t];
	if (within >= 0) {
		if (j == NULL || j->type != JSON_ARRAY) {
			fprintf(stderr, "%s:%d: expected array for coordinate container %d\n", fname, line, t);
			json_context(feature);
			return;
		}

		size_t i;
		for (i = 0; i < j->length; i++) {
			if (within == GEOM_POINT) {
				if (i == 0 || mb_geometry[t] == GEOM_MULTIPOINT) {
					op = VT_MOVETO;
				} else {
					op = VT_LINETO;
				}
			}

			parse_geometry(within, j->array[i], out, op, fname, line, feature, doing_attributes);
		}
	} else {
		if (doing_attributes) {
			draw d(op, 0, 0);

			if (j != NULL && j->type == JSON_HASH) {
				char *s = json_stringify(j);
				d.attributes = std::string(s);
				free(s);  // stringify
			} else {
				fprintf(stderr, "%s:%d: malformed point attribute\n", fname, line);
				json_context(j);
				json_context(feature);
			}

			out.push_back(d);
		} else if (j != NULL && j->type == JSON_ARRAY && j->length >= 2 && j->array[0]->type == JSON_NUMBER && j->array[1]->type == JSON_NUMBER) {
			long long x, y;
			double lon = j->array[0]->number;
			double lat = j->array[1]->number;
			projection->project(lon, lat, 32, &x, &y);

			if (j->length > 3) {
				static int warned = 0;

				if (!warned) {
					fprintf(stderr, "%s:%d: ignoring dimensions beyond three\n", fname, line);
					json_context(j);
					json_context(feature);
					warned = 1;
				}
			}

			draw d(op, x, y);

			for (size_t i = 2; i < j->length; i++) {
				if (j->array[i]->type == JSON_NUMBER) {
					d.elevations.push_back(j->array[i]->number);
				}
			}

			out.push_back(d);
		} else {
			fprintf(stderr, "%s:%d: malformed point\n", fname, line);
			json_context(j);
			json_context(feature);
		}
	}

	if (t == GEOM_POLYGON) {
		// Note that this is not using the correct meaning of closepath.
		//
		// We are using it here to close an entire Polygon, to distinguish
		// the Polygons within a MultiPolygon from each other.
		//
		// This will be undone in fix_polygon(), which needs to know which
		// rings come from which Polygons so that it can make the winding order
		// of the outer ring be the opposite of the order of the inner rings.

		out.push_back(draw(VT_CLOSEPATH, 0, 0));
	}
}

void merge_node_attributes(drawvec &geom, drawvec &attributes) {
	if (attributes.size() != geom.size()) {
		fprintf(stderr, "Geometry attributes don't match coordinates: %zu attributes for %zu geometries\n", attributes.size(), geom.size());
		exit(EXIT_FAILURE);
	}

	for (size_t i = 0; i < attributes.size(); i++) {
		if (geom[i].op != attributes[i].op) {
			fprintf(stderr, "Geometry attributes don't match coordinates: op %d vs %d\n", geom[i].op, attributes[i].op);
			exit(EXIT_FAILURE);
		}

		geom[i].attributes = attributes[i].attributes;
	}
}

void canonicalize(json_object *o) {
	if (o->type == JSON_NUMBER) {
		std::string s;
		long long v;
		unsigned long long uv;

		if (is_integer(o->string, &v)) {
			s = std::to_string(v);
		} else if (is_unsigned_integer(o->string, &uv)) {
			s = std::to_string(uv);
		} else {
			s = milo::dtoa_milo(o->number);
		}
		free(o->string);
		o->string = strdup(s.c_str());
	} else if (o->type == JSON_HASH) {
		for (size_t i = 0; i < o->length; i++) {
			canonicalize(o->values[i]);
		}
	} else if (o->type == JSON_ARRAY) {
		for (size_t i = 0; i < o->length; i++) {
			canonicalize(o->array[i]);
		}
	}
}

void stringify_value(json_object *value, int &type, std::string &stringified, const char *reading, int line, json_object *feature) {
	if (value != NULL) {
		int vt = value->type;
		std::string val;

		if (vt == JSON_STRING || vt == JSON_NUMBER) {
			val = value->string;
		} else if (vt == JSON_TRUE) {
			val = "true";
		} else if (vt == JSON_FALSE) {
			val = "false";
		} else if (vt == JSON_NULL) {
			val = "null";
		} else {
			canonicalize(value);
			const char *v = json_stringify(value);
			val = std::string(v);
			free((void *) v);  // stringify
		}

		if (vt == JSON_STRING) {
			type = mvt_string;
			stringified = val;
			std::string err = check_utf8(val);
			if (err != "") {
				fprintf(stderr, "%s:%d: %s\n", reading, line, err.c_str());
				json_context(feature);
				exit(EXIT_FAILURE);
			}
		} else if (vt == JSON_NUMBER) {
			type = mvt_double;

			long long v;
			unsigned long long uv;

			if (is_integer(value->string, &v)) {
				stringified = std::to_string(v);
			} else if (is_unsigned_integer(value->string, &uv)) {
				stringified = std::to_string(uv);
			} else {
				stringified = milo::dtoa_milo(value->number);
			}
		} else if (vt == JSON_TRUE || vt == JSON_FALSE) {
			type = mvt_bool;
			stringified = val;
		} else if (vt == JSON_NULL) {
			type = mvt_null;
			stringified = "null";
		} else {
			type = mvt_hash;
			stringified = val;
		}
	}
}
