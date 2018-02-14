#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <zlib.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <vtzero/vector_tile.hpp>
#include <vtzero/builder.hpp>
#include <vtzero/index.hpp>
#include "mvt.hpp"
#include "geometry.hpp"
#include <protozero/varint.hpp>
#include <protozero/pbf_reader.hpp>
#include <protozero/pbf_writer.hpp>
#include "milo/dtoa_milo.h"

mvt_geometry::mvt_geometry(int nop, long long nx, long long ny) {
	this->op = nop;
	this->x = nx;
	this->y = ny;
}

// https://github.com/mapbox/mapnik-vector-tile/blob/master/src/vector_tile_compression.hpp
bool is_compressed(std::string const &data) {
	return data.size() > 2 && (((uint8_t) data[0] == 0x78 && (uint8_t) data[1] == 0x9C) || ((uint8_t) data[0] == 0x1F && (uint8_t) data[1] == 0x8B));
}

// https://github.com/mapbox/mapnik-vector-tile/blob/master/src/vector_tile_compression.hpp
int decompress(std::string const &input, std::string &output) {
	z_stream inflate_s;
	inflate_s.zalloc = Z_NULL;
	inflate_s.zfree = Z_NULL;
	inflate_s.opaque = Z_NULL;
	inflate_s.avail_in = 0;
	inflate_s.next_in = Z_NULL;
	if (inflateInit2(&inflate_s, 32 + 15) != Z_OK) {
		fprintf(stderr, "error: %s\n", inflate_s.msg);
	}
	inflate_s.next_in = (Bytef *) input.data();
	inflate_s.avail_in = input.size();
	size_t length = 0;
	do {
		output.resize(length + 2 * input.size());
		inflate_s.avail_out = 2 * input.size();
		inflate_s.next_out = (Bytef *) (output.data() + length);
		int ret = inflate(&inflate_s, Z_FINISH);
		if (ret != Z_STREAM_END && ret != Z_OK && ret != Z_BUF_ERROR) {
			fprintf(stderr, "error: %s\n", inflate_s.msg);
			return 0;
		}

		length += (2 * input.size() - inflate_s.avail_out);
	} while (inflate_s.avail_out == 0);
	inflateEnd(&inflate_s);
	output.resize(length);
	return 1;
}

// https://github.com/mapbox/mapnik-vector-tile/blob/master/src/vector_tile_compression.hpp
int compress(std::string const &input, std::string &output) {
	z_stream deflate_s;
	deflate_s.zalloc = Z_NULL;
	deflate_s.zfree = Z_NULL;
	deflate_s.opaque = Z_NULL;
	deflate_s.avail_in = 0;
	deflate_s.next_in = Z_NULL;
	deflateInit2(&deflate_s, Z_BEST_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
	deflate_s.next_in = (Bytef *) input.data();
	deflate_s.avail_in = input.size();
	size_t length = 0;
	do {
		size_t increase = input.size() / 2 + 1024;
		output.resize(length + increase);
		deflate_s.avail_out = increase;
		deflate_s.next_out = (Bytef *) (output.data() + length);
		int ret = deflate(&deflate_s, Z_FINISH);
		if (ret != Z_STREAM_END && ret != Z_OK && ret != Z_BUF_ERROR) {
			return -1;
		}
		length += (increase - deflate_s.avail_out);
	} while (deflate_s.avail_out == 0);
	deflateEnd(&deflate_s);
	output.resize(length);
	return 0;
}

struct geom_decoder {
	std::vector<mvt_geometry> geometry;
	int type;
	int state;

	void points_begin(const uint32_t) {
		type = mvt_point;
	}

	void points_point(const vtzero::point point) {
		geometry.push_back(mvt_geometry(mvt_moveto, point.x, point.y));
	}

	void points_end() {

	}

	void linestring_begin(const uint32_t) {
		type = mvt_linestring;
		state = mvt_moveto;
	}

	void linestring_point(const vtzero::point point) {
		geometry.push_back(mvt_geometry(state, point.x, point.y));
		state = mvt_lineto;
	}

	void linestring_end() {

	}

	void ring_begin(const uint32_t) {
		type = mvt_polygon;
		state = mvt_moveto;
	}

	void ring_point(const vtzero::point point) {
		geometry.push_back(mvt_geometry(state, point.x, point.y));
		state = mvt_lineto;
	}

	void ring_end(const vtzero::ring_type) {
		if (geometry.size() > 0) {
			geometry.pop_back();
		}

		geometry.push_back(mvt_geometry(mvt_closepath, 0, 0));
	}
};

bool mvt_tile::decode(std::string &message, bool &was_compressed) {
	layers.clear();
	std::string src;

	if (is_compressed(message)) {
		std::string uncompressed;
		decompress(message, uncompressed);
		src = uncompressed;
		was_compressed = true;
	} else {
		src = message;
		was_compressed = false;
	}

	protozero::pbf_reader reader(src);
	vtzero::vector_tile vtz{src};

	while (auto vtz_layer = vtz.next_layer()) {
		mvt_layer layer;

		layer.name = vtz_layer.name().to_string();
		layer.extent = vtz_layer.extent();
		layer.version = vtz_layer.version();

		while (auto vtz_feature = vtz_layer.next_feature()) {
			mvt_feature feature;

			if (vtz_feature.has_id()) {
				feature.has_id = true;
				feature.id = vtz_feature.id();
			}

			while (auto vtz_property = vtz_feature.next_property()) {
				std::string pkey = vtz_property.key().to_string();
				vtzero::property_value pvalue = vtz_property.value();
				mvt_value value;

				switch (pvalue.type()) {
				case vtzero::property_value_type::string_value:
					value.type = mvt_string;
					value.string_value = pvalue.string_value().to_string();
					break;

				case vtzero::property_value_type::double_value:
					value.type = mvt_double;
					value.numeric_value.double_value = pvalue.double_value();
					break;

				case vtzero::property_value_type::float_value:
					value.type = mvt_float;
					value.numeric_value.float_value = pvalue.float_value();
					break;

				case vtzero::property_value_type::int_value:
					value.type = mvt_int;
					value.numeric_value.int_value = pvalue.int_value();
					break;

				case vtzero::property_value_type::sint_value:
					value.type = mvt_sint;
					value.numeric_value.sint_value = pvalue.sint_value();
					break;

				case vtzero::property_value_type::uint_value:
					value.type = mvt_uint;
					value.numeric_value.uint_value = pvalue.uint_value();
					break;

				case vtzero::property_value_type::bool_value:
					value.type = mvt_bool;
					value.numeric_value.bool_value = pvalue.bool_value();
					break;

				default:
					fprintf(stderr, "Unknown attribute type in tile\n");
					exit(EXIT_FAILURE);
				}

				layer.tag(feature, pkey, value);
			}

			geom_decoder gd;
			vtzero::decode_geometry(vtz_feature.geometry(), gd);
			feature.geometry = gd.geometry;
			feature.type = gd.type;

			layer.features.push_back(feature);
		}

		layers.push_back(layer);
	}

	return true;
}

void copy_attrs(mvt_layer &layer, mvt_feature &feature, vtzero::key_index<std::unordered_map> &index, vtzero::feature_builder &out) {
	for (size_t i = 0; i + 1 < feature.tags.size(); i += 2) {
		std::string key = layer.keys[feature.tags[i]];
		mvt_value &pbv = layer.values[feature.tags[i + 1]];

		const auto k = index(key);

		if (pbv.type == mvt_string) {
			out.add_property(k, pbv.string_value);
		} else if (pbv.type == mvt_float) {
			out.add_property(k, pbv.numeric_value.float_value);
		} else if (pbv.type == mvt_double) {
			out.add_property(k, pbv.numeric_value.double_value);
		} else if (pbv.type == mvt_int) {
			out.add_property(k, pbv.numeric_value.int_value);
		} else if (pbv.type == mvt_uint) {
			out.add_property(k, pbv.numeric_value.uint_value);
		} else if (pbv.type == mvt_sint) {
			out.add_property(k, pbv.numeric_value.sint_value);
		} else if (pbv.type == mvt_bool) {
			out.add_property(k, pbv.numeric_value.bool_value);
		}
	}

	if (feature.has_id) {
		out.set_id(feature.id);
	}
}

std::string mvt_tile::encode() {
	vtzero::tile_builder vtz_tile;

	for (size_t i = 0; i < layers.size(); i++) {
		vtzero::layer_builder vtz_layer{vtz_tile, layers[i].name, (unsigned) layers[i].version, (unsigned) layers[i].extent};
		vtzero::key_index<std::unordered_map> vtz_index{vtz_layer};

		// vtz_layer.set_version(layers[i].version);
		// vtz_layer.set_extent(layers[i].extent);

		for (size_t j = 0; j < layers[i].features.size(); j++) {
			if (layers[i].features[j].type == mvt_point) {
				vtzero::point_feature_builder vtz_feature{vtz_layer};

				for (size_t k = 0; k < layers[i].features[j].geometry.size(); k++) {
					vtz_feature.add_point(layers[i].features[j].geometry[k].x,
					                      layers[i].features[j].geometry[k].y);
				}

				copy_attrs(layers[i], layers[i].features[j], vtz_index, vtz_feature);
				vtz_feature.commit();
			} else if (layers[i].features[j].type == mvt_linestring) {
				vtzero::linestring_feature_builder vtz_feature{vtz_layer};
				mvt_feature &f = layers[i].features[j];

				for (size_t k = 0; k < f.geometry.size(); k++) {
					if (f.geometry[k].op == mvt_moveto) {
						size_t l;
						for (l = k + 1; l < f.geometry.size(); l++) {
							if (f.geometry[l].op != mvt_lineto) {
								break;
							}
						}

						vtz_feature.add_linestring(l - k);
						for (size_t m = k; m < l; m++) {
							vtz_feature.set_point(f.geometry[m].x, f.geometry[m].y);
						}

						k = l - 1;
					}
				}

				copy_attrs(layers[i], layers[i].features[j], vtz_index, vtz_feature);
				vtz_feature.commit();
			} else if (layers[i].features[j].type == mvt_polygon) {
				vtzero::polygon_feature_builder vtz_feature{vtz_layer};
				mvt_feature &f = layers[i].features[j];

				for (size_t k = 0; k < f.geometry.size(); k++) {
					if (f.geometry[k].op == mvt_moveto) {
						size_t l;
						for (l = k + 1; l < f.geometry.size(); l++) {
							if (f.geometry[l].op != mvt_lineto) {
								break;
							}
						}

						vtz_feature.add_ring(l - k);
						for (size_t m = k; m < l; m++) {
							vtz_feature.set_point(f.geometry[m].x, f.geometry[m].y);
						}

						k = l - 1;
					}
				}

				copy_attrs(layers[i], layers[i].features[j], vtz_index, vtz_feature);
				vtz_feature.commit();
			}
		}
	}

	std::string data = vtz_tile.serialize();

#if 0
	protozero::pbf_writer writer(data);

	for (size_t i = 0; i < layers.size(); i++) {
		std::string layer_string;
		protozero::pbf_writer layer_writer(layer_string);

		layer_writer.add_uint32(15, layers[i].version); /* version */
		layer_writer.add_string(1, layers[i].name);     /* name */
		layer_writer.add_uint32(5, layers[i].extent);   /* extent */

		for (size_t j = 0; j < layers[i].keys.size(); j++) {
			layer_writer.add_string(3, layers[i].keys[j]); /* key */
		}

		for (size_t v = 0; v < layers[i].values.size(); v++) {
			std::string value_string;
			protozero::pbf_writer value_writer(value_string);
			mvt_value &pbv = layers[i].values[v];

			if (pbv.type == mvt_string) {
				value_writer.add_string(1, pbv.string_value);
			} else if (pbv.type == mvt_float) {
				value_writer.add_float(2, pbv.numeric_value.float_value);
			} else if (pbv.type == mvt_double) {
				value_writer.add_double(3, pbv.numeric_value.double_value);
			} else if (pbv.type == mvt_int) {
				value_writer.add_int64(4, pbv.numeric_value.int_value);
			} else if (pbv.type == mvt_uint) {
				value_writer.add_uint64(5, pbv.numeric_value.uint_value);
			} else if (pbv.type == mvt_sint) {
				value_writer.add_sint64(6, pbv.numeric_value.sint_value);
			} else if (pbv.type == mvt_bool) {
				value_writer.add_bool(7, pbv.numeric_value.bool_value);
			}

			layer_writer.add_message(4, value_string);
		}

		for (size_t f = 0; f < layers[i].features.size(); f++) {
			std::string feature_string;
			protozero::pbf_writer feature_writer(feature_string);

			feature_writer.add_enum(3, layers[i].features[f].type);
			feature_writer.add_packed_uint32(2, std::begin(layers[i].features[f].tags), std::end(layers[i].features[f].tags));

			if (layers[i].features[f].has_id) {
				feature_writer.add_uint64(1, layers[i].features[f].id);
			}

			std::vector<uint32_t> geometry;

			int px = 0, py = 0;
			int cmd_idx = -1;
			int cmd = -1;
			int length = 0;

			std::vector<mvt_geometry> &geom = layers[i].features[f].geometry;

			for (size_t g = 0; g < geom.size(); g++) {
				int op = geom[g].op;

				if (op != cmd) {
					if (cmd_idx >= 0) {
						geometry[cmd_idx] = (length << 3) | (cmd & ((1 << 3) - 1));
					}

					cmd = op;
					length = 0;
					cmd_idx = geometry.size();
					geometry.push_back(0);
				}

				if (op == mvt_moveto || op == mvt_lineto) {
					long long wwx = geom[g].x;
					long long wwy = geom[g].y;

					int dx = wwx - px;
					int dy = wwy - py;

					geometry.push_back(protozero::encode_zigzag32(dx));
					geometry.push_back(protozero::encode_zigzag32(dy));

					px = wwx;
					py = wwy;
					length++;
				} else if (op == mvt_closepath) {
					length++;
				} else {
					fprintf(stderr, "\nInternal error: corrupted geometry\n");
					exit(EXIT_FAILURE);
				}
			}

			if (cmd_idx >= 0) {
				geometry[cmd_idx] = (length << 3) | (cmd & ((1 << 3) - 1));
			}

			feature_writer.add_packed_uint32(4, std::begin(geometry), std::end(geometry));
			layer_writer.add_message(2, feature_string);
		}

		writer.add_message(3, layer_string);
	}
#endif

	return data;
}

bool mvt_value::operator<(const mvt_value &o) const {
	if (type < o.type) {
		return true;
	}
	if (type == o.type) {
		if ((type == mvt_string && string_value < o.string_value) ||
		    (type == mvt_float && numeric_value.float_value < o.numeric_value.float_value) ||
		    (type == mvt_double && numeric_value.double_value < o.numeric_value.double_value) ||
		    (type == mvt_int && numeric_value.int_value < o.numeric_value.int_value) ||
		    (type == mvt_uint && numeric_value.uint_value < o.numeric_value.uint_value) ||
		    (type == mvt_sint && numeric_value.sint_value < o.numeric_value.sint_value) ||
		    (type == mvt_bool && numeric_value.bool_value < o.numeric_value.bool_value)) {
			return true;
		}
	}

	return false;
}

static std::string quote(std::string const &s) {
	std::string buf;

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

	return buf;
}

std::string mvt_value::toString() {
	if (type == mvt_string) {
		return quote(string_value);
	} else if (type == mvt_int) {
		return std::to_string(numeric_value.int_value);
	} else if (type == mvt_double) {
		double v = numeric_value.double_value;
		if (v == (long long) v) {
			return std::to_string((long long) v);
		} else {
			return milo::dtoa_milo(v);
		}
	} else if (type == mvt_float) {
		double v = numeric_value.float_value;
		if (v == (long long) v) {
			return std::to_string((long long) v);
		} else {
			return milo::dtoa_milo(v);
		}
	} else if (type == mvt_sint) {
		return std::to_string(numeric_value.sint_value);
	} else if (type == mvt_uint) {
		return std::to_string(numeric_value.uint_value);
	} else if (type == mvt_bool) {
		return numeric_value.bool_value ? "true" : "false";
	} else {
		return "unknown";
	}
}

void mvt_layer::tag(mvt_feature &feature, std::string key, mvt_value value) {
	size_t ko, vo;

	std::map<std::string, size_t>::iterator ki = key_map.find(key);
	std::map<mvt_value, size_t>::iterator vi = value_map.find(value);

	if (ki == key_map.end()) {
		ko = keys.size();
		keys.push_back(key);
		key_map.insert(std::pair<std::string, size_t>(key, ko));
	} else {
		ko = ki->second;
	}

	if (vi == value_map.end()) {
		vo = values.size();
		values.push_back(value);
		value_map.insert(std::pair<mvt_value, size_t>(value, vo));
	} else {
		vo = vi->second;
	}

	feature.tags.push_back(ko);
	feature.tags.push_back(vo);
}

bool is_integer(const char *s, long long *v) {
	errno = 0;
	char *endptr;

	*v = strtoll(s, &endptr, 0);
	if (*v == 0 && errno != 0) {
		return 0;
	}
	if ((*v == LLONG_MIN || *v == LLONG_MAX) && (errno == ERANGE || errno == EINVAL)) {
		return 0;
	}
	if (*endptr != '\0') {
		// Special case: If it is an integer followed by .0000 or similar,
		// it is still an integer

		if (*endptr != '.') {
			return 0;
		}
		endptr++;
		for (; *endptr != '\0'; endptr++) {
			if (*endptr != '0') {
				return 0;
			}
		}

		return 1;
	}

	return 1;
}

bool is_unsigned_integer(const char *s, unsigned long long *v) {
	errno = 0;
	char *endptr;

	// Special check because MacOS stroull() returns 1
	// for -18446744073709551615
	while (isspace(*s)) {
		s++;
	}
	if (*s == '-') {
		return 0;
	}

	*v = strtoull(s, &endptr, 0);
	if (*v == 0 && errno != 0) {
		return 0;
	}
	if ((*v == ULLONG_MAX) && (errno == ERANGE || errno == EINVAL)) {
		return 0;
	}
	if (*endptr != '\0') {
		// Special case: If it is an integer followed by .0000 or similar,
		// it is still an integer

		if (*endptr != '.') {
			return 0;
		}
		endptr++;
		for (; *endptr != '\0'; endptr++) {
			if (*endptr != '0') {
				return 0;
			}
		}

		return 1;
	}

	return 1;
}

mvt_value stringified_to_mvt_value(int type, const char *s) {
	mvt_value tv;

	if (type == mvt_double) {
		long long v;
		unsigned long long uv;
		if (is_unsigned_integer(s, &uv)) {
			if (uv <= LLONG_MAX) {
				tv.type = mvt_int;
				tv.numeric_value.int_value = uv;
			} else {
				tv.type = mvt_uint;
				tv.numeric_value.uint_value = uv;
			}
		} else if (is_integer(s, &v)) {
			tv.type = mvt_sint;
			tv.numeric_value.sint_value = v;
		} else {
			errno = 0;
			char *endptr;

			float f = strtof(s, &endptr);

			if (endptr == s || ((f == HUGE_VAL || f == HUGE_VALF || f == HUGE_VALL) && errno == ERANGE)) {
				double d = strtod(s, &endptr);
				if (endptr == s || ((d == HUGE_VAL || d == HUGE_VALF || d == HUGE_VALL) && errno == ERANGE)) {
					fprintf(stderr, "Warning: numeric value %s could not be represented\n", s);
				}
				tv.type = mvt_double;
				tv.numeric_value.double_value = d;
			} else {
				double d = atof(s);
				if (f == d) {
					tv.type = mvt_float;
					tv.numeric_value.float_value = f;
				} else {
					// Conversion succeeded, but lost precision, so use double
					tv.type = mvt_double;
					tv.numeric_value.double_value = d;
				}
			}
		}
	} else if (type == mvt_bool) {
		tv.type = mvt_bool;
		tv.numeric_value.bool_value = (s[0] == 't');
	} else if (type == mvt_null) {
		tv.type = mvt_null;
	} else {
		tv.type = mvt_string;
		tv.string_value = s;
	}

	return tv;
}
