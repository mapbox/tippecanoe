// for vasprintf() on Linux
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include "projection.hpp"
#include "geometry.hpp"
#include "mvt.hpp"
#include "write_json.hpp"
#include "text.hpp"
#include "milo/dtoa_milo.h"

void json_writer::json_adjust() {
	if (state.size() == 0) {
		state.push_back(JSON_WRITE_TOP);
	} else if (state[state.size() - 1] == JSON_WRITE_TOP) {
		addc('\n');
		state[state.size() - 1] = JSON_WRITE_TOP;
	} else if (state[state.size() - 1] == JSON_WRITE_HASH) {
		if (!nospace) {
			addc(' ');
		}
		nospace = false;
		state[state.size() - 1] = JSON_WRITE_HASH_KEY;
	} else if (state[state.size() - 1] == JSON_WRITE_HASH_KEY) {
		adds(": ");
		state[state.size() - 1] = JSON_WRITE_HASH_VALUE;
	} else if (state[state.size() - 1] == JSON_WRITE_HASH_VALUE) {
		if (wantnl) {
			adds(",\n");
			nospace = false;
		} else if (nospace) {
			addc(',');
			nospace = false;
		} else {
			adds(", ");
		}
		wantnl = false;
		state[state.size() - 1] = JSON_WRITE_HASH_KEY;
	} else if (state[state.size() - 1] == JSON_WRITE_ARRAY) {
		if (!nospace) {
			addc(' ');
		}
		nospace = false;
		state[state.size() - 1] = JSON_WRITE_ARRAY_ELEMENT;
	} else if (state[state.size() - 1] == JSON_WRITE_ARRAY_ELEMENT) {
		if (wantnl) {
			adds(",\n");
			nospace = false;
		} else if (nospace) {
			addc(',');
			nospace = false;
		} else {
			adds(", ");
		}
		wantnl = false;
		state[state.size() - 1] = JSON_WRITE_ARRAY_ELEMENT;
	} else {
		fprintf(stderr, "Impossible JSON state\n");
		exit(EXIT_FAILURE);
	}
}

void json_writer::json_write_array() {
	json_adjust();
	addc('[');

	state.push_back(JSON_WRITE_ARRAY);
}

void json_writer::json_end_array() {
	if (state.size() == 0) {
		fprintf(stderr, "End JSON array at top level\n");
		exit(EXIT_FAILURE);
	}

	json_write_tok tok = state[state.size() - 1];
	state.pop_back();

	if (tok == JSON_WRITE_ARRAY || tok == JSON_WRITE_ARRAY_ELEMENT) {
		if (!nospace) {
			addc(' ');
		}
		nospace = false;
		addc(']');
	} else {
		fprintf(stderr, "End JSON array with unexpected state\n");
		exit(EXIT_FAILURE);
	}
}

void json_writer::json_write_hash() {
	json_adjust();
	addc('{');

	state.push_back(JSON_WRITE_HASH);
}

void json_writer::json_end_hash() {
	if (state.size() == 0) {
		fprintf(stderr, "End JSON hash at top level\n");
		exit(EXIT_FAILURE);
	}

	json_write_tok tok = state[state.size() - 1];
	state.pop_back();

	if (tok == JSON_WRITE_HASH) {
		if (!nospace) {
			adds("  ");  // Preserve accidental extra space from before
		}
		nospace = false;
		addc('}');
	} else if (tok == JSON_WRITE_HASH_VALUE) {
		if (!nospace) {
			addc(' ');
		}
		nospace = false;
		addc('}');
	} else {
		fprintf(stderr, "End JSON hash with unexpected state\n");
		exit(EXIT_FAILURE);
	}
}

void json_writer::json_write_string(std::string const &str) {
	json_adjust();

	addc('"');
	for (size_t i = 0; i < str.size(); i++) {
		if (str[i] == '\\' || str[i] == '"') {
			aprintf("\\%c", str[i]);
		} else if ((unsigned char) str[i] < ' ') {
			aprintf("\\u%04x", str[i]);
		} else {
			addc(str[i]);
		}
	}
	addc('"');
}

void json_writer::json_write_number(double d) {
	json_adjust();

	adds(milo::dtoa_milo(d).c_str());
}

// Just to avoid json_writer:: changing expected output format
void json_writer::json_write_float(double d) {
	json_adjust();

	aprintf("%f", d);
}

void json_writer::json_write_unsigned(unsigned long long v) {
	json_adjust();

	aprintf("%llu", v);
}

void json_writer::json_write_signed(long long v) {
	json_adjust();

	aprintf("%lld", v);
}

void json_writer::json_write_stringified(std::string const &str) {
	json_adjust();

	adds(str);
}

void json_writer::json_write_bool(bool b) {
	json_adjust();

	if (b) {
		adds("true");
	} else {
		adds("false");
	}
}

void json_writer::json_write_null() {
	json_adjust();

	adds("null");
}

void json_writer::json_write_newline() {
	addc('\n');
	nospace = true;
}

void json_writer::json_comma_newline() {
	wantnl = true;
}

void json_writer::aprintf(const char *format, ...) {
	va_list ap;
	char *tmp;

	va_start(ap, format);
	if (vasprintf(&tmp, format, ap) < 0) {
		fprintf(stderr, "memory allocation failure\n");
		exit(EXIT_FAILURE);
	}
	va_end(ap);

	adds(std::string(tmp, strlen(tmp)));
	free(tmp);
}

void json_writer::addc(char c) {
	if (f != NULL) {
		putc(c, f);
	} else if (s != NULL) {
		s->push_back(c);
	}
}

void json_writer::adds(std::string const &str) {
	if (f != NULL) {
		fputs(str.c_str(), f);
	} else if (s != NULL) {
		s->append(str);
	}
}

struct lonlat {
	int op;
	double lon;
	double lat;
	long long x;
	long long y;
	std::vector<double> elevations;
	std::string attribute;

	lonlat(int nop, double nlon, double nlat, long long nx, long long ny, std::vector<double> nelevations, std::string nattribute)
	    : op(nop),
	      lon(nlon),
	      lat(nlat),
	      x(nx),
	      y(ny),
	      elevations(nelevations),
	      attribute(nattribute) {
	}
};

void write_kv(json_writer &state, const char *key, mvt_value const &val) {
	if (val.type == mvt_string) {
		state.json_write_string(key);
		state.json_write_string(val.string_value);
	} else if (val.type == mvt_int) {
		state.json_write_string(key);
		state.json_write_signed(val.numeric_value.int_value);
	} else if (val.type == mvt_double) {
		state.json_write_string(key);
		state.json_write_number(val.numeric_value.double_value);
	} else if (val.type == mvt_float) {
		state.json_write_string(key);
		state.json_write_number(val.numeric_value.float_value);
	} else if (val.type == mvt_sint) {
		state.json_write_string(key);
		state.json_write_signed(val.numeric_value.sint_value);
	} else if (val.type == mvt_uint) {
		state.json_write_string(key);
		state.json_write_unsigned(val.numeric_value.uint_value);
	} else if (val.type == mvt_bool) {
		state.json_write_string(key);
		state.json_write_bool(val.numeric_value.bool_value);
	} else if (val.type == mvt_null) {
		state.json_write_string(key);
		state.json_write_null();
	} else {
		fprintf(stderr, "Internal error: property with unknown type\n");
		exit(EXIT_FAILURE);
	}
}

static std::string quote(std::string const &s) {
	std::string buf;
	buf.push_back('"');

	for (size_t i = 0; i < s.size(); i++) {
		unsigned char ch = s[i];

		if (ch == '\\' || ch == '\"') {
			buf.push_back('\\');
			buf.push_back(ch);
		} else if (ch < ' ') {
			char tmp[7];
			sprintf(tmp, "\\u%04x", ch);
			buf.append(std::string(tmp));
		} else {
			buf.push_back(ch);
		}
	}

	buf.push_back('"');
	return buf;
}

void stringify_val(std::string &out, mvt_value const &val) {
	if (val.type == mvt_string) {
		out.append(quote(val.string_value));
	} else {
		out.append(val.toString());
	}
}

void print_val(mvt_value const &val, json_writer &state) {
	std::string s;
	stringify_val(s, val);
	state.json_write_stringified(s);
}

void write_coordinates(json_writer &state, lonlat const &p) {
	state.json_write_array();

	state.json_write_float(p.lon);
	state.json_write_float(p.lat);

	for (size_t i = 0; i < p.elevations.size(); i++) {
		state.json_write_number(p.elevations[i]);
	}

	state.json_end_array();
}

void write_attributes(json_writer &state, lonlat const &p) {
	if (p.attribute.size() != 0) {
		state.json_write_stringified(p.attribute);
	} else {
		state.json_write_hash();
		state.json_end_hash();
	}
}

struct coordinate_writer {
	std::string tag;
	void (*function)(json_writer &state, lonlat const &p);
};

std::vector<std::string> decode_node_attributes(mvt_feature const &feature, const mvt_layer &layer) {
	std::vector<mvt_geometry> const &geom = feature.geometry;
	std::vector<unsigned long> const &attr = feature.node_attributes;
	std::vector<std::string> out;

	for (size_t t = 0; t + 1 < attr.size(); t++) {
		if (attr[t] >= layer.keys.size()) {
			fprintf(stderr, "Out of bounds attribute reference %lu into %zu\n", attr[t], layer.keys.size());
			exit(EXIT_FAILURE);
		}

		std::string key = layer.keys[attr[t]];

		t++;
		mvt_value const &val = layer.decode_property(attr, t);

		if (val.type != mvt_list) {
			fprintf(stderr, "Expected node attribute to be a list\n");
			exit(EXIT_FAILURE);
		}

		if (val.list_value.size() != geom.size()) {
			fprintf(stderr, "Node attribute list size doesn't match geometry size\n");
			exit(EXIT_FAILURE);
		}

		for (size_t g = 0; g < geom.size(); g++) {
			out.push_back(std::string("{") + quote(key) + ":" + val.list_value[g].toString() + "}");
		}
	}

	return out;
}

void layer_to_geojson(mvt_layer const &layer, unsigned z, unsigned x, unsigned y, bool comma, bool name, bool zoom, bool dropped, unsigned long long index, long long sequence, long long extent, bool complain, json_writer &state) {
	for (size_t f = 0; f < layer.features.size(); f++) {
		mvt_feature const &feat = layer.features[f];

		state.json_write_hash();
		state.json_write_string("type");
		state.json_write_string("Feature");

		if (feat.has_id) {
			state.json_write_string("id");
			state.json_write_unsigned(feat.id);
		}
		if (feat.string_id.size() != 0) {
			state.json_write_string("id");
			state.json_write_string(feat.string_id);
		}

		if (name || zoom || index != 0 || sequence != 0 || extent != 0) {
			state.json_write_string("tippecanoe");
			state.json_write_hash();

			if (name) {
				state.json_write_string("layer");
				state.json_write_string(layer.name);
			}

			if (zoom) {
				state.json_write_string("minzoom");
				state.json_write_unsigned(z);

				state.json_write_string("maxzoom");
				state.json_write_unsigned(z);
			}

			if (dropped) {
				state.json_write_string("dropped");
				state.json_write_bool(feat.dropped);
			}

			if (index != 0) {
				state.json_write_string("index");
				state.json_write_unsigned(index);
			}

			if (sequence != 0) {
				state.json_write_string("sequence");
				state.json_write_signed(sequence);
			}

			if (extent != 0) {
				state.json_write_string("extent");
				state.json_write_signed(extent);
			}

			state.json_end_hash();
		}

		state.json_write_string("properties");
		state.json_write_hash();

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

			state.json_write_string(key);
			print_val(val, state);
		}

		for (size_t t = 0; t + 1 < feat.properties.size(); t++) {
			if (feat.properties[t] >= layer.keys.size()) {
				fprintf(stderr, "Out of bounds attribute reference %lu into %zu\n", feat.properties[t], layer.keys.size());
				exit(EXIT_FAILURE);
			}
			const char *key = layer.keys[feat.properties[t]].c_str();

			t++;
			mvt_value const &val = layer.decode_property(feat.properties, t);

			state.json_write_string(key);
			print_val(val, state);
		}

		state.json_end_hash();

		state.json_write_string("geometry");
		state.json_write_hash();

		std::vector<lonlat> ops;
		bool has_attributes = false;

		std::vector<std::string> attributes = decode_node_attributes(feat, layer);

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

				ops.push_back(lonlat(op, lon, lat, px, py, feat.geometry[g].elevations, g >= attributes.size() ? "" : attributes[g]));
				if (g < attributes.size() && attributes[g].size() != 0) {
					has_attributes = true;
				}
			} else {
				ops.push_back(lonlat(op, 0, 0, 0, 0, std::vector<double>(), ""));
			}
		}

		std::vector<coordinate_writer> coordinate_writers;
		{
			coordinate_writer coord_writer;
			coord_writer.tag = "coordinates";
			coord_writer.function = write_coordinates;
			coordinate_writers.push_back(coord_writer);

			if (has_attributes) {
				coordinate_writer attrib_writer;
				attrib_writer.tag = "attributes";
				attrib_writer.function = write_attributes;
				coordinate_writers.push_back(attrib_writer);
			}

			if (feat.knots.size() != 0) {
				state.json_write_string("degree");
				state.json_write_number(feat.spline_degree);

				state.json_write_string("knots");
				state.json_write_array();

				size_t here = 0;
				while (here < feat.knots.size()) {
					mvt_value v = layer.decode_property(feat.knots, here);
					here++;

					state.json_write_stringified(v.toString());
				}

				state.json_end_array();
			}
		}

		if (feat.type == VT_POINT) {
			if (ops.size() == 1) {
				state.json_write_string("type");
				state.json_write_string("Point");

				for (auto c : coordinate_writers) {
					state.json_write_string(c.tag);
					c.function(state, ops[0]);
				}
			} else {
				state.json_write_string("type");
				state.json_write_string("MultiPoint");

				for (auto c : coordinate_writers) {
					state.json_write_string(c.tag);
					state.json_write_array();

					for (size_t i = 0; i < ops.size(); i++) {
						c.function(state, ops[i]);
					}

					state.json_end_array();
				}
			}
		} else if (feat.type == VT_LINE || feat.type == VT_SPLINE) {
			int movetos = 0;
			for (size_t i = 0; i < ops.size(); i++) {
				if (ops[i].op == VT_MOVETO) {
					movetos++;
				}
			}

			if (movetos < 2) {
				state.json_write_string("type");

				if (feat.type == VT_LINE) {
					state.json_write_string("LineString");
				} else {
					state.json_write_string("Spline");
				}

				for (auto c : coordinate_writers) {
					state.json_write_string(c.tag);
					state.json_write_array();

					for (size_t i = 0; i < ops.size(); i++) {
						c.function(state, ops[i]);
					}

					state.json_end_array();
				}
			} else {
				state.json_write_string("type");

				if (feat.type == VT_LINE) {
					state.json_write_string("MultiLineString");
				} else {
					state.json_write_string("MultiSpline");
				}

				for (auto c : coordinate_writers) {
					state.json_write_string(c.tag);
					state.json_write_array();
					state.json_write_array();

					int sstate = 0;
					for (size_t i = 0; i < ops.size(); i++) {
						if (ops[i].op == VT_MOVETO) {
							if (sstate == 0) {
								c.function(state, ops[i]);

								sstate = 1;
							} else {
								state.json_end_array();
								state.json_write_array();

								c.function(state, ops[i]);

								sstate = 1;
							}
						} else {
							c.function(state, ops[i]);
						}
					}

					state.json_end_array();
					state.json_end_array();
				}
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

				// fprintf("\"area\": %Lf,", area);
			}

			if (outer > 1) {
				state.json_write_string("type");
				state.json_write_string("MultiPolygon");

				state.json_write_string("coordinates");
				state.json_write_array();
				state.json_write_array();
				state.json_write_array();
			} else {
				state.json_write_string("type");
				state.json_write_string("Polygon");

				state.json_write_string("coordinates");
				state.json_write_array();
				state.json_write_array();
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
						state.json_end_array();
						state.json_end_array();

						state.json_write_array();
						state.json_write_array();
					}
					sstate = 1;
				}

				if (sstate == 2) {
					// new ring in the same polygon
					state.json_end_array();
					state.json_write_array();
				}

				for (size_t j = 0; j < rings[i].size(); j++) {
					if (rings[i][j].op != VT_CLOSEPATH) {
						state.json_write_array();
						state.json_write_float(rings[i][j].lon);
						state.json_write_float(rings[i][j].lat);
						state.json_end_array();
					} else {
						state.json_write_array();
						state.json_write_float(rings[i][0].lon);
						state.json_write_float(rings[i][0].lat);
						state.json_end_array();
					}
				}

				sstate = 2;
			}

			if (outer > 1) {
				state.json_end_array();
				state.json_end_array();
				state.json_end_array();
			} else {
				state.json_end_array();
				state.json_end_array();
			}
		}

		state.json_end_hash();
		state.json_end_hash();

		if (comma) {
			state.json_write_newline();
			state.json_comma_newline();
		}
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
