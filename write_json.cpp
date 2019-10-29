// for vasprintf() on Linux
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <map>
#include <string>
#include "projection.hpp"
#include "geometry.hpp"
#include "mvt.hpp"
#include "write_json.hpp"
#include "milo/dtoa_milo.h"

void json_writer::json_adjust() {
	if (jw.size() == 0) {
		jw.push_back(JSON_WRITE_TOP);
	} else if (jw[jw.size() - 1] == JSON_WRITE_TOP) {
		addc('\n');
		jw[jw.size() - 1] = JSON_WRITE_TOP;
	} else if (jw[jw.size() - 1] == JSON_WRITE_HASH) {
		if (!nospace) {
			addc(' ');
		}
		nospace = false;
		jw[jw.size() - 1] = JSON_WRITE_HASH_KEY;
	} else if (jw[jw.size() - 1] == JSON_WRITE_HASH_KEY) {
		adds(": ");
		jw[jw.size() - 1] = JSON_WRITE_HASH_VALUE;
	} else if (jw[jw.size() - 1] == JSON_WRITE_HASH_VALUE) {
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
		jw[jw.size() - 1] = JSON_WRITE_HASH_KEY;
	} else if (jw[jw.size() - 1] == JSON_WRITE_ARRAY) {
		if (!nospace) {
			addc(' ');
		}
		nospace = false;
		jw[jw.size() - 1] = JSON_WRITE_ARRAY_ELEMENT;
	} else if (jw[jw.size() - 1] == JSON_WRITE_ARRAY_ELEMENT) {
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
		jw[jw.size() - 1] = JSON_WRITE_ARRAY_ELEMENT;
	} else {
		fprintf(stderr, "Impossible JSON jw\n");
		exit(EXIT_FAILURE);
	}
}

void json_writer::begin_array() {
	json_adjust();
	addc('[');

	jw.push_back(JSON_WRITE_ARRAY);
}

void json_writer::end_array() {
	if (jw.size() == 0) {
		fprintf(stderr, "End JSON array at top level\n");
		exit(EXIT_FAILURE);
	}

	write_tok tok = jw[jw.size() - 1];
	jw.pop_back();

	if (tok == JSON_WRITE_ARRAY || tok == JSON_WRITE_ARRAY_ELEMENT) {
		if (!nospace) {
			addc(' ');
		}
		nospace = false;
		addc(']');
	} else {
		fprintf(stderr, "End JSON array with unexpected jw\n");
		exit(EXIT_FAILURE);
	}
}

void json_writer::begin_hash() {
	json_adjust();
	addc('{');

	jw.push_back(JSON_WRITE_HASH);
}

void json_writer::end_hash() {
	if (jw.size() == 0) {
		fprintf(stderr, "End JSON hash at top level\n");
		exit(EXIT_FAILURE);
	}

	write_tok tok = jw[jw.size() - 1];
	jw.pop_back();

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
		fprintf(stderr, "End JSON hash with unexpected jw\n");
		exit(EXIT_FAILURE);
	}
}

void json_writer::write_string(std::string const &str) {
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

void json_writer::write_number(double d) {
	json_adjust();

	adds(milo::dtoa_milo(d).c_str());
}

// Just to avoid json_writer:: changing expected output format
void json_writer::write_float(double d) {
	json_adjust();

	aprintf("%f", d);
}

void json_writer::write_unsigned(unsigned long long v) {
	json_adjust();

	aprintf("%llu", v);
}

void json_writer::write_signed(long long v) {
	json_adjust();

	aprintf("%lld", v);
}

void json_writer::write_stringified(std::string const &str) {
	json_adjust();

	adds(str);
}

void json_writer::write_bool(bool b) {
	json_adjust();

	if (b) {
		adds("true");
	} else {
		adds("false");
	}
}

void json_writer::write_null() {
	json_adjust();

	adds("null");
}

void json_writer::write_newline() {
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

	lonlat(int nop, double nlon, double nlat, long long nx, long long ny)
	    : op(nop),
	      lon(nlon),
	      lat(nlat),
	      x(nx),
	      y(ny) {
	}
};

void layer_to_geojson(mvt_layer const &layer, unsigned z, unsigned x, unsigned y, bool comma, bool name, bool zoom, bool dropped, unsigned long long index, long long sequence, long long extent, bool complain, json_writer &jw) {
	for (size_t f = 0; f < layer.features.size(); f++) {
		mvt_feature const &feat = layer.features[f];

		jw.begin_hash();
		jw.write_string("type");
		jw.write_string("Feature");

		if (feat.has_id) {
			jw.write_string("id");
			jw.write_unsigned(feat.id);
		}

		if (name || zoom || index != 0 || sequence != 0 || extent != 0) {
			jw.write_string("tippecanoe");
			jw.begin_hash();

			if (name) {
				jw.write_string("layer");
				jw.write_string(layer.name);
			}

			if (zoom) {
				jw.write_string("minzoom");
				jw.write_unsigned(z);

				jw.write_string("maxzoom");
				jw.write_unsigned(z);
			}

			if (dropped) {
				jw.write_string("dropped");
				jw.write_bool(feat.dropped);
			}

			if (index != 0) {
				jw.write_string("index");
				jw.write_unsigned(index);
			}

			if (sequence != 0) {
				jw.write_string("sequence");
				jw.write_signed(sequence);
			}

			if (extent != 0) {
				jw.write_string("extent");
				jw.write_signed(extent);
			}

			jw.end_hash();
		}

		jw.write_string("properties");
		jw.begin_hash();

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
				jw.write_string(key);
				jw.write_string(val.string_value);
			} else if (val.type == mvt_int) {
				jw.write_string(key);
				jw.write_signed(val.numeric_value.int_value);
			} else if (val.type == mvt_double) {
				jw.write_string(key);
				jw.write_number(val.numeric_value.double_value);
			} else if (val.type == mvt_float) {
				jw.write_string(key);
				jw.write_number(val.numeric_value.float_value);
			} else if (val.type == mvt_sint) {
				jw.write_string(key);
				jw.write_signed(val.numeric_value.sint_value);
			} else if (val.type == mvt_uint) {
				jw.write_string(key);
				jw.write_unsigned(val.numeric_value.uint_value);
			} else if (val.type == mvt_bool) {
				jw.write_string(key);
				jw.write_bool(val.numeric_value.bool_value);
			} else if (val.type == mvt_null) {
				jw.write_string(key);
				jw.write_null();
			} else {
				fprintf(stderr, "Internal error: property with unknown type\n");
				exit(EXIT_FAILURE);
			}
		}

		jw.end_hash();

		jw.write_string("geometry");
		jw.begin_hash();

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
				jw.write_string("type");
				jw.write_string("Point");

				jw.write_string("coordinates");

				jw.begin_array();
				jw.write_float(ops[0].lon);
				jw.write_float(ops[0].lat);
				jw.end_array();
			} else {
				jw.write_string("type");
				jw.write_string("MultiPoint");

				jw.write_string("coordinates");
				jw.begin_array();

				for (size_t i = 0; i < ops.size(); i++) {
					jw.begin_array();
					jw.write_float(ops[i].lon);
					jw.write_float(ops[i].lat);
					jw.end_array();
				}

				jw.end_array();
			}
		} else if (feat.type == VT_LINE) {
			int movetos = 0;
			for (size_t i = 0; i < ops.size(); i++) {
				if (ops[i].op == VT_MOVETO) {
					movetos++;
				}
			}

			if (movetos < 2) {
				jw.write_string("type");
				jw.write_string("LineString");

				jw.write_string("coordinates");
				jw.begin_array();

				for (size_t i = 0; i < ops.size(); i++) {
					jw.begin_array();
					jw.write_float(ops[i].lon);
					jw.write_float(ops[i].lat);
					jw.end_array();
				}

				jw.end_array();
			} else {
				jw.write_string("type");
				jw.write_string("MultiLineString");

				jw.write_string("coordinates");
				jw.begin_array();
				jw.begin_array();

				int sstate = 0;
				for (size_t i = 0; i < ops.size(); i++) {
					if (ops[i].op == VT_MOVETO) {
						if (sstate == 0) {
							jw.begin_array();
							jw.write_float(ops[i].lon);
							jw.write_float(ops[i].lat);
							jw.end_array();

							sstate = 1;
						} else {
							jw.end_array();
							jw.begin_array();

							jw.begin_array();
							jw.write_float(ops[i].lon);
							jw.write_float(ops[i].lat);
							jw.end_array();

							sstate = 1;
						}
					} else {
						jw.begin_array();
						jw.write_float(ops[i].lon);
						jw.write_float(ops[i].lat);
						jw.end_array();
					}
				}

				jw.end_array();
				jw.end_array();
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
				jw.write_string("type");
				jw.write_string("MultiPolygon");

				jw.write_string("coordinates");
				jw.begin_array();
				jw.begin_array();
				jw.begin_array();
			} else {
				jw.write_string("type");
				jw.write_string("Polygon");

				jw.write_string("coordinates");
				jw.begin_array();
				jw.begin_array();
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
						jw.end_array();
						jw.end_array();

						jw.begin_array();
						jw.begin_array();
					}
					sstate = 1;
				}

				if (sstate == 2) {
					// new ring in the same polygon
					jw.end_array();
					jw.begin_array();
				}

				for (size_t j = 0; j < rings[i].size(); j++) {
					if (rings[i][j].op != VT_CLOSEPATH) {
						jw.begin_array();
						jw.write_float(rings[i][j].lon);
						jw.write_float(rings[i][j].lat);
						jw.end_array();
					} else {
						jw.begin_array();
						jw.write_float(rings[i][0].lon);
						jw.write_float(rings[i][0].lat);
						jw.end_array();
					}
				}

				sstate = 2;
			}

			if (outer > 1) {
				jw.end_array();
				jw.end_array();
				jw.end_array();
			} else {
				jw.end_array();
				jw.end_array();
			}
		}

		jw.end_hash();
		jw.end_hash();

		if (comma) {
			jw.write_newline();
			jw.json_comma_newline();
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
