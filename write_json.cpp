#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <string>
#include "projection.hpp"
#include "geometry.hpp"
#include "mvt.hpp"
#include "write_json.hpp"
#include "milo/dtoa_milo.h"

struct lonlat {
	int op;
	double lon;
	double lat;
	long long x;
	long long y;
	long long id;

	lonlat(int nop, double nlon, double nlat, long long nx, long long ny, long long nid)
	    : op(nop),
	      lon(nlon),
	      lat(nlat),
	      x(nx),
	      y(ny),
	      id(nid) {
	}

	void print(FILE *f) {
		fprintf(f, "[ %f, %f ]", lon, lat);
	}
};

void layer_to_geojson(FILE *fp, mvt_layer const &layer, unsigned z, unsigned x, unsigned y, bool comma, bool name, bool zoom, bool dropped, unsigned long long index, long long sequence, long long extent, bool complain) {
	for (size_t f = 0; f < layer.features.size(); f++) {
		mvt_feature const &feat = layer.features[f];

		if (comma && f != 0) {
			fprintf(fp, ",\n");
		}

		fprintf(fp, "{ \"type\": \"Feature\"");

		if (feat.has_id) {
			fprintf(fp, ", \"id\": %llu", feat.id);
		}

		if (name || zoom || index != 0 || sequence != 0 || extent != 0) {
			bool need_comma = false;

			fprintf(fp, ", \"tippecanoe\": { ");

			if (name) {
				if (need_comma) {
					fprintf(fp, ", ");
				}
				fprintf(fp, "\"layer\": ");
				fprintq(fp, layer.name.c_str());
				need_comma = true;
			}

			if (zoom) {
				if (need_comma) {
					fprintf(fp, ", ");
				}
				fprintf(fp, "\"minzoom\": %u, ", z);
				fprintf(fp, "\"maxzoom\": %u", z);
				need_comma = true;
			}

			if (dropped) {
				if (need_comma) {
					fprintf(fp, ", ");
				}
				fprintf(fp, "\"dropped\": %s", feat.dropped ? "true" : "false");
				need_comma = true;
			}

			if (index != 0) {
				if (need_comma) {
					fprintf(fp, ", ");
				}
				fprintf(fp, "\"index\": %llu", index);
				need_comma = true;
			}

			if (sequence != 0) {
				if (need_comma) {
					fprintf(fp, ", ");
				}
				fprintf(fp, "\"sequence\": %lld", sequence);
				need_comma = true;
			}

			if (extent != 0) {
				if (need_comma) {
					fprintf(fp, ", ");
				}
				fprintf(fp, "\"extent\": %lld", extent);
				need_comma = true;
			}

			fprintf(fp, " }");
		}

		fprintf(fp, ", \"properties\": { ");

		for (size_t t = 0; t + 1 < feat.tags.size(); t += 2) {
			if (t != 0) {
				fprintf(fp, ", ");
			}

			if (feat.tags[t] >= layer.keys.size()) {
				fprintf(stderr, "Error: out of bounds feature key (%u in %zu)\n", feat.tags[t], layer.keys.size());
				exit(EXIT_FAILURE);
			}
			if (feat.tags[t + 1] >= layer.values.size()) {
				fprintf(stderr, "Error: out of bounds feature value (%u in %zu)\n", feat.tags[t + 1], layer.values.size());
				exit(EXIT_FAILURE);
			}

			const char *key = layer.keys[feat.tags[t]].c_str();
			mvt_value const &val = layer.values[feat.tags[t + 1]];

			if (val.type == mvt_string) {
				fprintq(fp, key);
				fprintf(fp, ": ");
				fprintq(fp, val.string_value.c_str());
			} else if (val.type == mvt_int) {
				fprintq(fp, key);
				fprintf(fp, ": %lld", val.numeric_value.int_value);
			} else if (val.type == mvt_double) {
				fprintq(fp, key);
				double v = val.numeric_value.double_value;
				fprintf(fp, ": %s", milo::dtoa_milo(v).c_str());
			} else if (val.type == mvt_float) {
				fprintq(fp, key);
				double v = val.numeric_value.float_value;
				fprintf(fp, ": %s", milo::dtoa_milo(v).c_str());
			} else if (val.type == mvt_sint) {
				fprintq(fp, key);
				fprintf(fp, ": %lld", val.numeric_value.sint_value);
			} else if (val.type == mvt_uint) {
				fprintq(fp, key);
				fprintf(fp, ": %llu", val.numeric_value.uint_value);
			} else if (val.type == mvt_bool) {
				fprintq(fp, key);
				fprintf(fp, ": %s", val.numeric_value.bool_value ? "true" : "false");
			}
		}

		fprintf(fp, " }, \"geometry\": { ");

		std::vector<lonlat> ops;

		for (size_t g = 0; g < feat.geometry.size(); g++) {
			int op = feat.geometry[g].op;
			long long px = feat.geometry[g].x;
			long long py = feat.geometry[g].y;

			if (op == VT_MOVETO || op == VT_LINETO) {
				long long scale = 1LL << (32 - z);
				long long wx = scale * x + (scale / layer.extent) * px;
				long long wy = scale * y + (scale / layer.extent) * py;

				double lat, lon;
				projection->unproject(wx, wy, 32, &lon, &lat);

				ops.push_back(lonlat(op, lon, lat, px, py, feat.geometry[g].id));
			} else {
				ops.push_back(lonlat(op, 0, 0, 0, 0, feat.geometry[g].id));
			}
		}

		if (feat.type == VT_POINT) {
			if (ops.size() == 1) {
				fprintf(fp, "\"type\": \"Point\", \"coordinates\": ");
				ops[0].print(fp);
			} else {
				fprintf(fp, "\"type\": \"MultiPoint\", \"coordinates\": [ ");
				for (size_t i = 0; i < ops.size(); i++) {
					if (i != 0) {
						fprintf(fp, ", ");
					}
					ops[i].print(fp);
				}
				fprintf(fp, " ]");
			}
		} else if (feat.type == VT_LINE) {
			int movetos = 0;
			for (size_t i = 0; i < ops.size(); i++) {
				if (ops[i].op == VT_MOVETO) {
					movetos++;
				}
			}

			if (movetos < 2) {
				fprintf(fp, "\"type\": \"LineString\", \"coordinates\": [ ");
				for (size_t i = 0; i < ops.size(); i++) {
					if (i != 0) {
						fprintf(fp, ", ");
					}
					ops[i].print(fp);
				}
				fprintf(fp, " ]");
			} else {
				fprintf(fp, "\"type\": \"MultiLineString\", \"coordinates\": [ [ ");
				int state = 0;
				for (size_t i = 0; i < ops.size(); i++) {
					if (ops[i].op == VT_MOVETO) {
						if (state == 0) {
							ops[i].print(fp);
							state = 1;
						} else {
							fprintf(fp, " ], [ ");
							ops[i].print(fp);
							state = 1;
						}
					} else {
						fprintf(fp, ", ");
						ops[i].print(fp);
					}
				}
				fprintf(fp, " ] ]");
			}
		} else if (feat.type == VT_POLYGON) {
			std::vector<std::vector<lonlat> > rings;
			std::vector<double> areas;

			for (size_t i = 0; i < ops.size(); i++) {
				if (ops[i].op == VT_MOVETO) {
					rings.push_back(std::vector<lonlat>());
					areas.push_back(0);
				}

				int n = rings.size() - 1;
				if (n >= 0) {
					if (ops[i].op == VT_CLOSEPATH) {
						rings[n].push_back(rings[n][0]);
					} else {
						rings[n].push_back(ops[i]);
					}
				}

				if (i + 1 >= ops.size() || ops[i + 1].op == VT_MOVETO) {
					if (ops[i].op != VT_CLOSEPATH) {
						static bool warned = false;

						if (!warned) {
							fprintf(stderr, "Ring does not end with closepath (ends with %d)\n", ops[i].op);
							if (complain) {
								exit(EXIT_FAILURE);
							}

							warned = true;
						}
					}
				}
			}

			int outer = 0;

			for (size_t i = 0; i < rings.size(); i++) {
				long double area = 0;
				for (size_t k = 0; k < rings[i].size(); k++) {
					if (rings[i][k].op != VT_CLOSEPATH) {
						area += (long double) rings[i][k].x * (long double) rings[i][(k + 1) % rings[i].size()].y;
						area -= (long double) rings[i][k].y * (long double) rings[i][(k + 1) % rings[i].size()].x;
					}
				}
				area /= 2;

				areas[i] = area;
				if (areas[i] >= 0 || i == 0) {
					outer++;
				}

				// fprintf(fp, "\"area\": %Lf,", area);
			}

			if (outer > 1) {
				fprintf(fp, "\"type\": \"MultiPolygon\", \"coordinates\": [ [ [ ");
			} else {
				fprintf(fp, "\"type\": \"Polygon\", \"coordinates\": [ [ ");
			}

			int state = 0;
			for (size_t i = 0; i < rings.size(); i++) {
				if (i == 0 && areas[i] < 0) {
					static bool warned = false;

					if (!warned) {
						fprintf(stderr, "Polygon begins with an inner ring\n");
						if (complain) {
							exit(EXIT_FAILURE);
						}

						warned = true;
					}
				}

				if (areas[i] >= 0) {
					if (state != 0) {
						// new multipolygon
						fprintf(fp, " ] ], [ [ ");
					}
					state = 1;
				}

				if (state == 2) {
					// new ring in the same polygon
					fprintf(fp, " ], [ ");
				}

				for (size_t j = 0; j < rings[i].size(); j++) {
					if (rings[i][j].op != VT_CLOSEPATH) {
						if (j != 0) {
							fprintf(fp, ", ");
						}

						rings[i][j].print(fp);
					} else {
						if (j != 0) {
							fprintf(fp, ", ");
						}

						rings[i][0].print(fp);
					}
				}

				state = 2;
			}

			if (outer > 1) {
				fprintf(fp, " ] ] ]");
			} else {
				fprintf(fp, " ] ]");
			}
		}

		fprintf(fp, " } }\n");
	}
}

void fprintq(FILE *fp, const char *s) {
	fputc('"', fp);
	for (; *s; s++) {
		if (*s == '\\' || *s == '"') {
			fprintf(fp, "\\%c", *s);
		} else if (*s >= 0 && *s < ' ') {
			fprintf(fp, "\\u%04x", *s);
		} else {
			fputc(*s, fp);
		}
	}
	fputc('"', fp);
}
