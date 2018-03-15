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

static void json_adjust(FILE *f, json_write_state &state) {
	if (state.state.size() == 0) {
		state.state.push_back(JSON_WRITE_TOP);
	} else if (state.state[state.state.size() - 1] == JSON_WRITE_TOP) {
		fprintf(f, "\n");
		state.state[state.state.size() - 1] = JSON_WRITE_TOP;
	} else if (state.state[state.state.size() - 1] == JSON_WRITE_HASH) {
		fprintf(f, " ");
		state.state[state.state.size() - 1] = JSON_WRITE_HASH_KEY;
	} else if (state.state[state.state.size() - 1] == JSON_WRITE_HASH_KEY) {
		fprintf(f, ": ");
		state.state[state.state.size() - 1] = JSON_WRITE_HASH_VALUE;
	} else if (state.state[state.state.size() - 1] == JSON_WRITE_HASH_VALUE) {
		fprintf(f, ", ");
		state.state[state.state.size() - 1] = JSON_WRITE_HASH_KEY;
	} else if (state.state[state.state.size() - 1] == JSON_WRITE_ARRAY) {
		fprintf(f, " ");
		state.state[state.state.size() - 1] = JSON_WRITE_ARRAY_ELEMENT;
	} else if (state.state[state.state.size() - 1] == JSON_WRITE_ARRAY_ELEMENT) {
		fprintf(f, ", ");
		state.state[state.state.size() - 1] = JSON_WRITE_ARRAY_ELEMENT;
	} else {
		fprintf(stderr, "Impossible JSON state\n");
		exit(EXIT_FAILURE);
	}
}

void json_write_array(FILE *f, json_write_state &state) {
	json_adjust(f, state);
	fprintf(f, "[");

	state.state.push_back(JSON_WRITE_ARRAY);
}

void json_end_array(FILE *f, json_write_state &state) {
	if (state.state.size() == 0) {
		fprintf(stderr, "End JSON array at top level\n");
		exit(EXIT_FAILURE);
	}

	json_write_tok tok = state.state[state.state.size() - 1];
	state.state.pop_back();

	if (tok == JSON_WRITE_ARRAY || tok == JSON_WRITE_ARRAY_ELEMENT) {
		fprintf(f, " ]");
	} else {
		fprintf(stderr, "End JSON array with unexpected state\n");
		exit(EXIT_FAILURE);
	}
}

void json_write_hash(FILE *f, json_write_state &state) {
	json_adjust(f, state);
	fprintf(f, "{");

	state.state.push_back(JSON_WRITE_HASH);
}

void json_end_hash(FILE *f, json_write_state &state) {
	if (state.state.size() == 0) {
		fprintf(stderr, "End JSON hash at top level\n");
		exit(EXIT_FAILURE);
	}

	json_write_tok tok = state.state[state.state.size() - 1];
	state.state.pop_back();

	if (tok == JSON_WRITE_HASH) {
		fprintf(f, "  }");  // Preserve accidental extra space from before
	} else if (tok == JSON_WRITE_HASH_VALUE) {
		fprintf(f, " }");
	} else {
		fprintf(stderr, "End JSON hash with unexpected state\n");
		exit(EXIT_FAILURE);
	}
}

void json_write_string(FILE *f, std::string const &s, json_write_state &state) {
	json_adjust(f, state);

	putc('"', f);
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '\\' || s[i] == '"') {
			fprintf(f, "\\%c", s[i]);
		} else if ((unsigned char) s[i] < ' ') {
			fprintf(f, "\\u%04x", s[i]);
		} else {
			putc(s[i], f);
		}
	}
	putc('"', f);
}

void json_write_number(FILE *f, double d, json_write_state &state) {
	json_adjust(f, state);

	fputs(milo::dtoa_milo(d).c_str(), f);
}

// Just to avoid changing expected output format
void json_write_float(FILE *f, double d, json_write_state &state) {
	json_adjust(f, state);

	fprintf(f, "%f", d);
}

void json_write_unsigned(FILE *f, unsigned long long v, json_write_state &state) {
	json_adjust(f, state);

	fprintf(f, "%llu", v);
}

void json_write_signed(FILE *f, long long v, json_write_state &state) {
	json_adjust(f, state);

	fprintf(f, "%lld", v);
}

void json_write_stringified(FILE *f, std::string const &s, json_write_state &state) {
	json_adjust(f, state);

	fputs(s.c_str(), f);
}

void json_write_bool(FILE *f, bool b, json_write_state &state) {
	json_adjust(f, state);

	if (b) {
		fputs("true", f);
	} else {
		fputs("false", f);
	}
}

void json_write_null(FILE *f, json_write_state &state) {
	json_adjust(f, state);

	fputs("null", f);
}

struct lonlat {
	int op;
	double lon;
	double lat;
	long long x;
	long long y;

	lonlat(int nop, double nlon, double nlat, long long nx, long long ny)
	    : op(nop),
	      lon(nlon),
	      lat(nlat),
	      x(nx),
	      y(ny) {
	}
};

void layer_to_geojson(FILE *fp, mvt_layer const &layer, unsigned z, unsigned x, unsigned y, bool comma, bool name, bool zoom, bool dropped, unsigned long long index, long long sequence, long long extent, bool complain) {
	json_write_state state;

	for (size_t f = 0; f < layer.features.size(); f++) {
		mvt_feature const &feat = layer.features[f];

		if (comma && f != 0) {
			fprintf(fp, ",");
		}

		json_write_hash(fp, state);
		json_write_string(fp, "type", state);
		json_write_string(fp, "Feature", state);

		if (feat.has_id) {
			json_write_string(fp, "id", state);
			json_write_unsigned(fp, feat.id, state);
		}

		if (name || zoom || index != 0 || sequence != 0 || extent != 0) {
			json_write_string(fp, "tippecanoe", state);
			json_write_hash(fp, state);

			if (name) {
				json_write_string(fp, "layer", state);
				json_write_string(fp, layer.name, state);
			}

			if (zoom) {
				json_write_string(fp, "minzoom", state);
				json_write_unsigned(fp, z, state);

				json_write_string(fp, "maxzoom", state);
				json_write_unsigned(fp, z, state);
			}

			if (dropped) {
				json_write_string(fp, "dropped", state);
				json_write_bool(fp, feat.dropped, state);
			}

			if (index != 0) {
				json_write_string(fp, "index", state);
				json_write_unsigned(fp, index, state);
			}

			if (sequence != 0) {
				json_write_string(fp, "sequence", state);
				json_write_signed(fp, sequence, state);
			}

			if (extent != 0) {
				json_write_string(fp, "extent", state);
				json_write_signed(fp, extent, state);
			}

			json_end_hash(fp, state);
		}

		json_write_string(fp, "properties", state);
		json_write_hash(fp, state);

		for (size_t t = 0; t + 1 < feat.tags.size(); t += 2) {
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
				json_write_string(fp, key, state);
				json_write_string(fp, val.string_value, state);
			} else if (val.type == mvt_int) {
				json_write_string(fp, key, state);
				json_write_signed(fp, val.numeric_value.int_value, state);
			} else if (val.type == mvt_double) {
				json_write_string(fp, key, state);
				json_write_number(fp, val.numeric_value.double_value, state);
			} else if (val.type == mvt_float) {
				json_write_string(fp, key, state);
				json_write_number(fp, val.numeric_value.float_value, state);
			} else if (val.type == mvt_sint) {
				json_write_string(fp, key, state);
				json_write_signed(fp, val.numeric_value.sint_value, state);
			} else if (val.type == mvt_uint) {
				json_write_string(fp, key, state);
				json_write_unsigned(fp, val.numeric_value.uint_value, state);
			} else if (val.type == mvt_bool) {
				json_write_string(fp, key, state);
				json_write_bool(fp, val.numeric_value.bool_value, state);
			}
		}

		json_end_hash(fp, state);

		json_write_string(fp, "geometry", state);
		json_write_hash(fp, state);

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

				ops.push_back(lonlat(op, lon, lat, px, py));
			} else {
				ops.push_back(lonlat(op, 0, 0, 0, 0));
			}
		}

		if (feat.type == VT_POINT) {
			if (ops.size() == 1) {
				json_write_string(fp, "type", state);
				json_write_string(fp, "Point", state);

				json_write_string(fp, "coordinates", state);

				json_write_array(fp, state);
				json_write_float(fp, ops[0].lon, state);
				json_write_float(fp, ops[0].lat, state);
				json_end_array(fp, state);
			} else {
				json_write_string(fp, "type", state);
				json_write_string(fp, "MultiPoint", state);

				json_write_string(fp, "coordinates", state);
				json_write_array(fp, state);

				for (size_t i = 0; i < ops.size(); i++) {
					json_write_array(fp, state);
					json_write_float(fp, ops[i].lon, state);
					json_write_float(fp, ops[i].lat, state);
					json_end_array(fp, state);
				}

				json_end_array(fp, state);
			}
		} else if (feat.type == VT_LINE) {
			int movetos = 0;
			for (size_t i = 0; i < ops.size(); i++) {
				if (ops[i].op == VT_MOVETO) {
					movetos++;
				}
			}

			if (movetos < 2) {
				json_write_string(fp, "type", state);
				json_write_string(fp, "LineString", state);

				json_write_string(fp, "coordinates", state);
				json_write_array(fp, state);

				for (size_t i = 0; i < ops.size(); i++) {
					json_write_array(fp, state);
					json_write_float(fp, ops[i].lon, state);
					json_write_float(fp, ops[i].lat, state);
					json_end_array(fp, state);
				}

				json_end_array(fp, state);
			} else {
				json_write_string(fp, "type", state);
				json_write_string(fp, "MultiLineString", state);

				json_write_string(fp, "coordinates", state);
				json_write_array(fp, state);
				json_write_array(fp, state);

				int sstate = 0;
				for (size_t i = 0; i < ops.size(); i++) {
					if (ops[i].op == VT_MOVETO) {
						if (sstate == 0) {
							json_write_array(fp, state);
							json_write_float(fp, ops[i].lon, state);
							json_write_float(fp, ops[i].lat, state);
							json_end_array(fp, state);

							sstate = 1;
						} else {
							json_end_array(fp, state);
							json_write_array(fp, state);

							json_write_array(fp, state);
							json_write_float(fp, ops[i].lon, state);
							json_write_float(fp, ops[i].lat, state);
							json_end_array(fp, state);

							sstate = 1;
						}
					} else {
						json_write_array(fp, state);
						json_write_float(fp, ops[i].lon, state);
						json_write_float(fp, ops[i].lat, state);
						json_end_array(fp, state);
					}
				}

				json_end_array(fp, state);
				json_end_array(fp, state);
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
				json_write_string(fp, "type", state);
				json_write_string(fp, "MultiPolygon", state);

				json_write_string(fp, "coordinates", state);
				json_write_array(fp, state);
				json_write_array(fp, state);
				json_write_array(fp, state);
			} else {
				json_write_string(fp, "type", state);
				json_write_string(fp, "Polygon", state);

				json_write_string(fp, "coordinates", state);
				json_write_array(fp, state);
				json_write_array(fp, state);
			}

			int sstate = 0;
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
					if (sstate != 0) {
						// new multipolygon
						json_end_array(fp, state);
						json_end_array(fp, state);

						json_write_array(fp, state);
						json_write_array(fp, state);
					}
					sstate = 1;
				}

				if (sstate == 2) {
					// new ring in the same polygon
					json_end_array(fp, state);
					json_write_array(fp, state);
				}

				for (size_t j = 0; j < rings[i].size(); j++) {
					if (rings[i][j].op != VT_CLOSEPATH) {
						json_write_array(fp, state);
						json_write_float(fp, rings[i][j].lon, state);
						json_write_float(fp, rings[i][j].lat, state);
						json_end_array(fp, state);
					} else {
						json_write_array(fp, state);
						json_write_float(fp, rings[i][0].lon, state);
						json_write_float(fp, rings[i][0].lat, state);
						json_end_array(fp, state);
					}
				}

				sstate = 2;
			}

			if (outer > 1) {
				json_end_array(fp, state);
				json_end_array(fp, state);
				json_end_array(fp, state);
			} else {
				json_end_array(fp, state);
				json_end_array(fp, state);
			}
		}

		json_end_hash(fp, state);
		json_end_hash(fp, state);

		if (comma) {
			fprintf(fp, "\n");
		}
	}

	// XXX clean up newlines
	if (!comma) {
		fprintf(fp, "\n");
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
