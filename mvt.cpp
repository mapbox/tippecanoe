#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <zlib.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>
#include <cmath>
#include "mvt.hpp"
#include "geometry.hpp"
#include "protozero/varint.hpp"
#include "protozero/pbf_reader.hpp"
#include "protozero/pbf_writer.hpp"
#include "milo/dtoa_milo.h"
#include "jsonpull/jsonpull.h"

int mvt_format = mvt_blake;

mvt_geometry::mvt_geometry(int nop, long long nx, long long ny) {
	this->op = nop;
	this->x = nx;
	this->y = ny;
}

mvt_geometry::mvt_geometry(int nop, long long nx, long long ny, std::vector<double> nelevations) {
	this->op = nop;
	this->x = nx;
	this->y = ny;
	this->elevations = nelevations;
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
		fprintf(stderr, "Decompression error: %s\n", inflate_s.msg);
	}
	inflate_s.next_in = (Bytef *) input.data();
	inflate_s.avail_in = input.size();
	inflate_s.next_out = (Bytef *) output.data();
	inflate_s.avail_out = output.size();

	while (true) {
		size_t existing_output = inflate_s.next_out - (Bytef *) output.data();

		output.resize(existing_output + 2 * inflate_s.avail_in + 100);
		inflate_s.next_out = (Bytef *) output.data() + existing_output;
		inflate_s.avail_out = output.size() - existing_output;

		int ret = inflate(&inflate_s, 0);
		if (ret < 0) {
			fprintf(stderr, "Decompression error: ");
			if (ret == Z_DATA_ERROR) {
				fprintf(stderr, "data error");
			}
			if (ret == Z_STREAM_ERROR) {
				fprintf(stderr, "stream error");
			}
			if (ret == Z_MEM_ERROR) {
				fprintf(stderr, "out of memory");
			}
			if (ret == Z_BUF_ERROR) {
				fprintf(stderr, "no data in buffer");
			}
			fprintf(stderr, "\n");
			return 0;
		}

		if (ret == Z_STREAM_END) {
			break;
		}

		// ret must be Z_OK or Z_NEED_DICT;
		// continue decompresing
	}

	output.resize(inflate_s.next_out - (Bytef *) output.data());
	inflateEnd(&inflate_s);
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

mvt_scaling read_scaling(protozero::pbf_reader message) {
	protozero::pbf_reader dimension_reader(message);
	mvt_scaling dimension;

	dimension.offset = 0;
	dimension.multiplier = 1;
	dimension.base = 0;

	while (dimension_reader.next()) {
		switch (dimension_reader.tag()) {
		case 1:
			dimension.offset = dimension_reader.get_sint64();
			break;

		case 2:
			dimension.multiplier = dimension_reader.get_double();
			break;

		case 3:
			dimension.base = dimension_reader.get_double();
			break;

		default:
			dimension_reader.skip();
			break;
		}
	}

	return dimension;
}

bool mvt_tile::decode(std::string &message, bool &was_compressed) {
	layers.clear();
	std::string src;

	if (is_compressed(message)) {
		std::string uncompressed;
		if (decompress(message, uncompressed) == 0) {
			exit(EXIT_FAILURE);
		}
		src = uncompressed;
		was_compressed = true;
	} else {
		src = message;
		was_compressed = false;
	}

	protozero::pbf_reader reader(src);

	while (reader.next()) {
		switch (reader.tag()) {
		case 3: /* layer */
		{
			protozero::pbf_reader layer_reader(reader.get_message());
			mvt_layer layer;
			mvt_scaling elevation_scaling;

			while (layer_reader.next()) {
				switch (layer_reader.tag()) {
				case 1: /* name */
					layer.name = layer_reader.get_string();
					break;

				case 3: /* key */
					layer.keys.push_back(layer_reader.get_string());
					break;

				case 4: /* value */
				{
					protozero::pbf_reader value_reader(layer_reader.get_message());
					mvt_value value;

					value.type = mvt_null;
					value.numeric_value.null_value = 0;

					while (value_reader.next()) {
						switch (value_reader.tag()) {
						case 1: /* string */
							value.type = mvt_string;
							value.string_value = value_reader.get_string();
							break;

						case 2: /* float */
							value.type = mvt_float;
							value.numeric_value.float_value = value_reader.get_float();
							break;

						case 3: /* double */
							value.type = mvt_double;
							value.numeric_value.double_value = value_reader.get_double();
							break;

						case 4: /* int */
							value.type = mvt_int;
							value.numeric_value.int_value = value_reader.get_int64();
							break;

						case 5: /* uint */
							value.type = mvt_uint;
							value.numeric_value.uint_value = value_reader.get_uint64();
							break;

						case 6: /* sint */
							value.type = mvt_sint;
							value.numeric_value.sint_value = value_reader.get_sint64();
							break;

						case 7: /* bool */
							value.type = mvt_bool;
							value.numeric_value.bool_value = value_reader.get_bool();
							break;

						default:
							value_reader.skip();
							break;
						}
					}

					layer.values.push_back(value);
					break;
				}

				case 5: /* extent */
					layer.extent = layer_reader.get_uint32();
					break;

				case 6: /* string values */
					layer.string_values.push_back(layer_reader.get_string());
					break;

				case 7: /* floats */
				{
					auto pi = layer_reader.get_packed_float();
					for (auto it = pi.first; it != pi.second; ++it) {
						layer.float_values.push_back(*it);
					}
					break;
				}

				case 8: /* doubles */
				{
					auto pi = layer_reader.get_packed_double();
					for (auto it = pi.first; it != pi.second; ++it) {
						layer.double_values.push_back(*it);
					}
					break;
				}

				case 9: /* integers */
				{
					auto pi = layer_reader.get_packed_fixed64();
					for (auto it = pi.first; it != pi.second; ++it) {
						layer.uint64_values.push_back(*it);
					}
					break;
				}

				case 10: /* elevation scaling */
				{
					elevation_scaling = read_scaling(layer_reader.get_message());
					break;
				}

				case 11: /* elevation scaling */
				{
					layer.attribute_scalings.push_back(read_scaling(layer_reader.get_message()));
					break;
				}

				case 15: /* version */
					layer.version = layer_reader.get_uint32();
					break;

				case 2: /* feature */
				{
					protozero::pbf_reader feature_reader(layer_reader.get_message());
					mvt_feature feature;
					std::vector<uint32_t> geoms;

					while (feature_reader.next()) {
						switch (feature_reader.tag()) {
						case 1: /* id */
							feature.id = feature_reader.get_uint64();
							feature.has_id = true;
							break;

						case 2: /* tags */
						{
							auto pi = feature_reader.get_packed_uint32();
							for (auto it = pi.first; it != pi.second; ++it) {
								feature.tags.push_back(*it);
							}
							break;
						}

						case 3: /* feature type */
							feature.type = feature_reader.get_enum();
							break;

						case 4: /* geometry */
						{
							auto pi = feature_reader.get_packed_uint32();
							for (auto it = pi.first; it != pi.second; ++it) {
								geoms.push_back(*it);
							}
							break;
						}

						case 5: /* properties */
						{
							auto pi = feature_reader.get_packed_uint64();
							for (auto it = pi.first; it != pi.second; ++it) {
								feature.properties.push_back(*it);
							}
							break;
						}

						case 7: /* elevations */
						{
							auto pi = feature_reader.get_packed_sint64();
							for (auto it = pi.first; it != pi.second; ++it) {
								feature.elevations.push_back(*it);
							}
							break;
						}

						case 8: /* node attributes */
						{
							auto pi = feature_reader.get_packed_uint64();
							for (auto it = pi.first; it != pi.second; ++it) {
								feature.node_attributes.push_back(*it);
							}
							break;
						}

						case 10: /* string id */
						{
							feature.string_id = feature_reader.get_string();
							break;
						}

						case 11: /* knots */
						{
							auto pi = feature_reader.get_packed_double();
							for (auto it = pi.first; it != pi.second; ++it) {
								feature.knots.push_back(*it);
							}
							break;
						}

						default:
							feature_reader.skip();
							break;
						}
					}

					long long px = 0, py = 0;
					for (size_t g = 0; g < geoms.size(); g++) {
						uint32_t geom = geoms[g];
						uint32_t op = geom & 7;
						uint32_t count = geom >> 3;

						if (op == mvt_moveto || op == mvt_lineto) {
							for (size_t k = 0; k < count && g + 2 < geoms.size(); k++) {
								px += protozero::decode_zigzag32(geoms[g + 1]);
								py += protozero::decode_zigzag32(geoms[g + 2]);
								g += 2;

								mvt_geometry decoded = mvt_geometry(op, px, py);
								feature.geometry.push_back(decoded);
							}
						} else {
							feature.geometry.push_back(mvt_geometry(op, 0, 0));
						}
					}

					layer.features.push_back(feature);
					break;
				}

				default:
					layer_reader.skip();
					break;
				}
			}

			for (size_t i = 0; i < layer.keys.size(); i++) {
				layer.key_map.insert(std::pair<std::string, size_t>(layer.keys[i], i));
			}
			for (size_t i = 0; i < layer.values.size(); i++) {
				layer.value_map.insert(std::pair<mvt_value, size_t>(layer.values[i], i));
			}

			// This has to wait until the layer is decoded because we might not know
			// the values or elevation scales until after the features.

			for (size_t i = 0; i < layer.features.size(); i++) {
				std::vector<mvt_geometry> &geom = layer.features[i].geometry;
				std::vector<unsigned long> &attr = layer.features[i].node_attributes;
				size_t off = 0;

				if (attr.size() != 0) {
					for (size_t j = 0; j < geom.size(); j++) {
						if (geom[j].op == mvt_moveto || geom[j].op == mvt_lineto) {
							if (off < attr.size()) {
								mvt_value v = layer.decode_property(attr, off, true);
								off++;
								if (v.type == mvt_hash || v.type == mvt_list) {
									geom[j].attribute = v.string_value;
								} else if (v.type == mvt_null) {
									;
								} else {
									std::string s = v.toString();
									fprintf(stderr, "Found unexpected node attribute %s\n", s.c_str());
									exit(EXIT_FAILURE);
								}
							} else {
								fprintf(stderr, "Ran out of node attributes\n");
								exit(EXIT_FAILURE);
							}
						}
					}
				}

				attr.clear();

				long current_elevation = elevation_scaling.offset;
				off = 0;

				std::vector<long> &elevations = layer.features[i].elevations;
				if (elevations.size() != 0) {
					for (size_t j = 0; j < geom.size(); j++) {
						if (off < elevations.size()) {
							double el;

							current_elevation += elevations[off];
							el = elevation_scaling.base + elevation_scaling.multiplier * current_elevation;

							geom[j].elevations.push_back(el);

							off++;
						} else {
							fprintf(stderr, "Ran out of elevations\n");
							exit(EXIT_FAILURE);
						}
					}
				}

				elevations.clear();
			}

			layers.push_back(layer);
			break;
		}

		default:
			reader.skip();
			break;
		}
	}

	return true;
}

std::string mvt_tile::encode() {
	std::string data;

	protozero::pbf_writer writer(data);

	for (size_t i = 0; i < layers.size(); i++) {
		std::string layer_string;
		protozero::pbf_writer layer_writer(layer_string);
		bool layer_is3d = false;

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
			} else {
				fprintf(stderr, "Internal error: trying to write undefined attribute type to tile\n");
				exit(EXIT_FAILURE);
			}

			layer_writer.add_message(4, value_string);
		}

		for (size_t v = 0; v < layers[i].string_values.size(); v++) {
			layer_writer.add_string(6, layers[i].string_values[v]);
		}

		layer_writer.add_packed_float(7, std::begin(layers[i].float_values), std::end(layers[i].float_values));
		layer_writer.add_packed_double(8, std::begin(layers[i].double_values), std::end(layers[i].double_values));
		layer_writer.add_packed_fixed64(9, std::begin(layers[i].uint64_values), std::end(layers[i].uint64_values));

		mvt_scaling elevation_scaling;
		for (size_t f = 0; f < layers[i].features.size(); f++) {
			std::vector<mvt_geometry> &geom = layers[i].features[f].geometry;

			for (size_t g = 0; g < geom.size(); g++) {
				if (geom[g].elevations.size() > 0) {
					mvt_scaling dim;

					// XXX choose more appropriately
					dim.multiplier = 0.5;
					dim.base = -22.7;
					dim.offset = 10;

					elevation_scaling = dim;
				}
			}
		}

		for (size_t f = 0; f < layers[i].features.size(); f++) {
			std::string feature_string;
			protozero::pbf_writer feature_writer(feature_string);
			bool has_attributes = false;

			feature_writer.add_enum(3, layers[i].features[f].type);
			feature_writer.add_packed_uint32(2, std::begin(layers[i].features[f].tags), std::end(layers[i].features[f].tags));
			feature_writer.add_packed_uint64(5, std::begin(layers[i].features[f].properties), std::end(layers[i].features[f].properties));

			if (layers[i].features[f].has_id) {
				feature_writer.add_uint64(1, layers[i].features[f].id);
			}
			if (layers[i].features[f].string_id.size() != 0) {
				feature_writer.add_string(10, layers[i].features[f].string_id);
			}

			std::vector<long> elevations;
			long current_elevation = elevation_scaling.offset;

			int px = 0, py = 0;
			int cmd_idx = -1;
			int cmd = -1;
			int length = 0;

			std::vector<mvt_geometry> &geom = layers[i].features[f].geometry;
			bool feature_is3d = false;

			std::vector<uint32_t> geometry;
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

					if (geom[g].attributes.size() > 0) {
						has_attributes = true;
					}

					double el;
					if (geom[g].elevations.size() > 0) {
						el = geom[g].elevations[0];
						feature_is3d = true;
						layer_is3d = true;
					} else {
						el = 0;  // XXX detect
					}

					el = std::round((el - elevation_scaling.base) / elevation_scaling.multiplier);
					int64_t delta = el - current_elevation;

					elevations.push_back(delta);
					current_elevation += delta;
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

			if (feature_is3d) {
				feature_writer.add_packed_sint64(7, std::begin(elevations), std::end(elevations));
			}

			feature_writer.add_packed_double(11, std::begin(layers[i].features[f].knots), std::end(layers[i].features[f].knots));

			if (has_attributes > 0) {
				std::vector<unsigned long> attributes;

				for (size_t g = 0; g < geom.size(); g++) {
					int op = geom[g].op;
					if (op == mvt_moveto || op == mvt_lineto) {
						if (geom[g].attributes.size() > 0) {
							for (size_t e = 0; e < geom[g].attributes.size(); e++) {
								attributes.push_back(geom[g].attributes[e]);
							}
						} else {
							attributes.push_back((2 << 4) | 7);  // null
						}
					}
				}

				feature_writer.add_packed_uint64(8, std::begin(attributes), std::end(attributes));
			}

			layer_writer.add_message(2, feature_string);
		}

		if (layer_is3d) {
			std::string dimension_string;
			protozero::pbf_writer dimension_writer(dimension_string);

			dimension_writer.add_sint64(1, elevation_scaling.offset);
			dimension_writer.add_double(2, elevation_scaling.multiplier);
			dimension_writer.add_double(3, elevation_scaling.base);

			layer_writer.add_message(10, dimension_string);
		}

		writer.add_message(3, layer_string);
	}

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
		    (type == mvt_bool && numeric_value.bool_value < o.numeric_value.bool_value) ||
		    (type == mvt_null && numeric_value.null_value < o.numeric_value.null_value)) {
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

std::string mvt_value::toString() const {
	if (type == mvt_string) {
		return std::string("\"") + quote(string_value) + "\"";
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
	} else if ((type == mvt_list || type == mvt_hash) && string_value.size() > 0) {
		return string_value;
	} else if (type == mvt_null) {
		return "null";
	} else {
		return "unknown";
	}
}

size_t mvt_layer::tag_key(std::string const &key) {
	size_t ko;

	std::map<std::string, size_t>::iterator ki = key_map.find(key);

	if (ki == key_map.end()) {
		ko = keys.size();
		keys.push_back(key);
		key_map.insert(std::pair<std::string, size_t>(key, ko));
	} else {
		ko = ki->second;
	}

	return ko;
}

size_t tag_object(mvt_layer &layer, json_object *j) {
	mvt_value tv;

	if (j->type == JSON_NUMBER) {
		long long v;
		if (is_integer(j->string, &v)) {
			if (v >= 0) {
				tv.type = mvt_int;
				tv.numeric_value.int_value = v;
			} else {
				tv.type = mvt_sint;
				tv.numeric_value.sint_value = v;
			}
		} else {
			tv.type = mvt_double;
			tv.numeric_value.double_value = atof(j->string);
		}
	} else if (j->type == JSON_TRUE) {
		tv.type = mvt_bool;
		tv.numeric_value.bool_value = 1;
	} else if (j->type == JSON_FALSE) {
		tv.type = mvt_bool;
		tv.numeric_value.bool_value = 0;
	} else if (j->type == JSON_STRING) {
		tv.type = mvt_string;
		tv.string_value = std::string(j->string);
	} else if (j->type == JSON_NULL) {
		tv.type = mvt_null;
		tv.numeric_value.null_value = 0;
	}

	return layer.tag_value(tv);
}

size_t mvt_layer::tag_value(mvt_value const &value) {
	size_t vo;

	std::map<mvt_value, size_t>::iterator vi = value_map.find(value);

	if (vi == value_map.end()) {
		vo = values.size();
		values.push_back(value);
		value_map.insert(std::pair<mvt_value, size_t>(value, vo));
	} else {
		vo = vi->second;
	}

	return vo;
}

void mvt_layer::tag(mvt_feature &feature, std::string key, mvt_value value) {
	if (value.type == mvt_hash) {
		json_pull *jp = json_begin_string((char *) value.string_value.c_str());
		json_object *jo = json_read_tree(jp);
		if (jo == NULL) {
			fprintf(stderr, "Internal error: failed to reconstruct JSON %s\n", value.string_value.c_str());
			exit(EXIT_FAILURE);
		}
		size_t ko = tag_key(key);
		size_t vo = tag_object(*this, jo);
		feature.tags.push_back(ko);
		feature.tags.push_back(vo);
		json_free(jo);
		json_end(jp);
	} else {
		feature.tags.push_back(tag_key(key));
		feature.tags.push_back(tag_value(value));
	}
}

size_t mvt_layer::tag_v3_key(std::string key) {
	std::map<std::string, size_t>::iterator ki = key_map.find(key);
	size_t ko;

	if (ki == key_map.end()) {
		ko = keys.size();
		keys.push_back(key);
		key_map.insert(std::pair<std::string, size_t>(key, ko));
	} else {
		ko = ki->second;
	}

	return ko;
}

void tag_object_v3(mvt_layer &layer, json_object *j, std::vector<unsigned long> &onto) {
	mvt_value tv;

	if (j->type == JSON_NUMBER) {
		long long v;
		if (is_integer(j->string, &v)) {
			if (v >= 0) {
				tv.type = mvt_int;
				tv.numeric_value.int_value = v;
			} else {
				tv.type = mvt_sint;
				tv.numeric_value.sint_value = v;
			}
		} else {
			tv.type = mvt_double;
			tv.numeric_value.double_value = atof(j->string);
		}

		layer.tag_v3_value(tv, onto);
	} else if (j->type == JSON_TRUE) {
		tv.type = mvt_bool;
		tv.numeric_value.bool_value = 1;
		layer.tag_v3_value(tv, onto);
	} else if (j->type == JSON_FALSE) {
		tv.type = mvt_bool;
		tv.numeric_value.bool_value = 0;
		layer.tag_v3_value(tv, onto);
	} else if (j->type == JSON_STRING) {
		tv.type = mvt_string;
		tv.string_value = std::string(j->string);
		layer.tag_v3_value(tv, onto);
	} else if (j->type == JSON_NULL) {
		tv.type = mvt_null;
		tv.numeric_value.null_value = 0;
		layer.tag_v3_value(tv, onto);
	} else if (j->type == JSON_HASH) {
		unsigned long vo = 9 | (j->length << 4);
		onto.push_back(vo);

		for (size_t i = 0; i < j->length; i++) {
			if (j->keys[i]->type != JSON_STRING) {
				fprintf(stderr, "Internal error: hash key is not a string\n");
				exit(EXIT_FAILURE);
			}

			tv.type = mvt_string;
			tv.string_value = j->keys[i]->string;

			onto.push_back(layer.tag_v3_key(tv.string_value));
			tag_object_v3(layer, j->values[i], onto);
		}
	} else if (j->type == JSON_ARRAY) {
		unsigned long vo = 8 | (j->length << 4);
		onto.push_back(vo);

		for (size_t i = 0; i < j->length; i++) {
			tag_object_v3(layer, j->array[i], onto);
		}
	} else {
		fprintf(stderr, "Internal error: unknown JSON type\n");
		exit(EXIT_FAILURE);
	}
}

void mvt_layer::tag_v3_value(mvt_value value, std::vector<unsigned long> &onto) {
	std::map<mvt_value, unsigned long>::iterator vi = property_map.find(value);
	unsigned long vo;

	if (vi == property_map.end()) {
		if (value.type == mvt_string) {
			vo = (string_values.size() << 4) | 0;
			string_values.push_back(value.string_value);
			onto.push_back(vo);
		} else if (value.type == mvt_float) {
			vo = (float_values.size() << 4) | 1;
			float_values.push_back(value.numeric_value.float_value);
			onto.push_back(vo);
		} else if (value.type == mvt_double) {
			vo = (double_values.size() << 4) | 2;
			double_values.push_back(value.numeric_value.double_value);
			onto.push_back(vo);
		} else if (value.type == mvt_uint) {
			if (value.numeric_value.uint_value <= (1L << 61) - 1) {
				vo = (value.numeric_value.uint_value << 4) | 5;
			} else {
				vo = (uint64_values.size() << 4) | 3;
				uint64_values.push_back(value.numeric_value.uint_value);
			}
			onto.push_back(vo);
		} else if (value.type == mvt_int || value.type == mvt_sint) {
			long val;
			if (value.type == mvt_int) {
				val = value.numeric_value.int_value;
			} else {
				val = value.numeric_value.sint_value;
			}

			if (val >= -(1L << 60) + 1 && val <= (1L << 60) - 1) {
				vo = (protozero::encode_zigzag64(val) << 4) | 6;
			} else {
				vo = (uint64_values.size() << 4) | 4;
				uint64_values.push_back(protozero::encode_zigzag64(val));
			}
			onto.push_back(vo);
		} else if (value.type == mvt_bool) {
			vo = (value.numeric_value.bool_value << 4) | 7;
			onto.push_back(vo);
		} else if (value.type == mvt_null) {
			vo = (2 << 4) | 7;
			onto.push_back(vo);
		} else {
			fprintf(stderr, "Internal error: unknown value type %d\n", value.type);
			exit(EXIT_FAILURE);
		}

		property_map.insert(std::pair<mvt_value, size_t>(value, vo));
	} else {
		vo = vi->second;
		onto.push_back(vo);
	}
}

void mvt_layer::tag_v3(mvt_feature &feature, std::string key, mvt_value value) {
	size_t ko = tag_v3_key(key);
	feature.properties.push_back(ko);

	if (value.type == mvt_hash) {
		json_pull *jp = json_begin_string((char *) value.string_value.c_str());
		json_object *jo = json_read_tree(jp);
		if (jo == NULL) {
			fprintf(stderr, "Internal error: failed to reconstruct JSON %s\n", value.string_value.c_str());
			exit(EXIT_FAILURE);
		}
		tag_object_v3(*this, jo, feature.properties);
		json_free(jo);
		json_end(jp);
	} else {
		tag_v3_value(value, feature.properties);
	}
}

void mvt_layer::reorder_values() {
	std::vector<mvt_value> orig_values = values;
	std::vector<std::string> orig_keys = keys;

	std::sort(values.begin(), values.end());
	std::sort(keys.begin(), keys.end());

	std::map<mvt_value, size_t> new_value_map;
	for (size_t i = 0; i < values.size(); i++) {
		new_value_map.insert(std::pair<mvt_value, size_t>(values[i], i));
	}

	std::map<std::string, size_t> new_key_map;
	for (size_t i = 0; i < keys.size(); i++) {
		new_key_map.insert(std::pair<std::string, size_t>(keys[i], i));
	}

	for (size_t i = 0; i < features.size(); i++) {
		for (size_t j = 1; j < features[i].tags.size(); j += 2) {
			mvt_value v = orig_values[features[i].tags[j]];
			auto f = new_value_map.find(v);

			if (f == new_value_map.end()) {
				std::string vs = v.toString();
				fprintf(stderr, "Internal error: value %s was lost\n", vs.c_str());
				exit(EXIT_FAILURE);
			}

			features[i].tags[j] = f->second;
		}

		for (size_t j = 0; j < features[i].tags.size(); j += 2) {
			std::string k = orig_keys[features[i].tags[j]];
			auto f = new_key_map.find(k);

			if (f == new_key_map.end()) {
				fprintf(stderr, "Internal error: key %s was lost\n", k.c_str());
				exit(EXIT_FAILURE);
			}

			features[i].tags[j] = f->second;
		}
	}
}

mvt_value mvt_layer::decode_property(std::vector<unsigned long> const &property, size_t &off, bool stringify_nested) const {
	int type = property[off] & 0x0F;
	mvt_value ret;

	switch (type) {
	case 0: /* string reference */
		ret.type = mvt_string;
		if (property[off] >> 4 >= string_values.size()) {
			fprintf(stderr, "Out of bounds string reference: %lu vs %zu\n", property[off] >> 4, string_values.size());
			exit(EXIT_FAILURE);
		}
		ret.string_value = string_values[property[off] >> 4];
		return ret;

	case 1: /* float reference */
		ret.type = mvt_float;
		if (property[off] >> 4 >= float_values.size()) {
			fprintf(stderr, "Out of bounds float reference: %lu vs %zu\n", property[off] >> 4, float_values.size());
			exit(EXIT_FAILURE);
		}
		ret.numeric_value.float_value = float_values[property[off] >> 4];
		return ret;

	case 2: /* double reference */
		ret.type = mvt_double;
		if (property[off] >> 4 >= double_values.size()) {
			fprintf(stderr, "Out of bounds double reference: %lu vs %zu\n", property[off] >> 4, double_values.size());
			exit(EXIT_FAILURE);
		}
		ret.numeric_value.double_value = double_values[property[off] >> 4];
		return ret;

	case 3: /* unsigned int reference */
		ret.type = mvt_uint;
		if (property[off] >> 4 >= uint64_values.size()) {
			fprintf(stderr, "Out of bounds uint reference: %lu vs %zu\n", property[off] >> 4, uint64_values.size());
			exit(EXIT_FAILURE);
		}
		ret.numeric_value.uint_value = uint64_values[property[off] >> 4];
		return ret;

	case 4: /* signed int reference */
		ret.type = mvt_sint;
		if (property[off] >> 4 >= uint64_values.size()) {
			fprintf(stderr, "Out of bounds sint reference: %lu vs %zu\n", property[off] >> 4, uint64_values.size());
			exit(EXIT_FAILURE);
		}
		ret.numeric_value.sint_value = protozero::decode_zigzag64(uint64_values[property[off] >> 4]);
		return ret;

	case 5: /* unsigned integer */
		ret.type = mvt_uint;
		ret.numeric_value.uint_value = property[off] >> 4;
		return ret;

	case 6: /* signed integer */
		ret.type = mvt_sint;
		ret.numeric_value.sint_value = protozero::decode_zigzag64(property[off] >> 4);
		return ret;

	case 7: /* boolean */
		if ((property[off] >> 4) == 2) {
			ret.type = mvt_null;
			ret.numeric_value.null_value = 0;
		} else {
			ret.type = mvt_bool;
			ret.numeric_value.bool_value = property[off] >> 4;
		}
		return ret;

	case 8: /* list */
	{
		ret.type = mvt_list;
		size_t len = property[off] >> 4;
		off++;

		if (stringify_nested) {
			ret.string_value = "[";
		}

		for (size_t i = 0; i < len; i++) {
			mvt_value v1 = decode_property(property, off, stringify_nested);
			off++;

			if (stringify_nested) {
				ret.string_value.append(v1.toString());

				if (i + 1 < len) {
					ret.string_value.push_back(',');
				}
			} else {
				ret.list_value.push_back(v1);
			}
		}

		if (stringify_nested) {
			ret.string_value.append("]");
		}

		off--;  // so caller can increment
		return ret;
	}

	case 9: /* hash */
	{
		ret.type = mvt_hash;
		size_t len = property[off] >> 4;
		off++;

		if (stringify_nested) {
			ret.string_value = "{";
		}

		for (size_t i = 0; i < len; i++) {
			if (property[off] >= keys.size()) {
				fprintf(stderr, "Out of bounds hash key reference\n");
				exit(EXIT_FAILURE);
			}

			mvt_value v1;
			v1.type = mvt_string;
			v1.string_value = keys[property[off]];
			off++;

			mvt_value v2 = decode_property(property, off, stringify_nested);
			off++;

			if (stringify_nested) {
				ret.string_value.append(v1.toString());
				ret.string_value.append(":");
				ret.string_value.append(v2.toString());

				if (i + 1 < len) {
					ret.string_value.push_back(',');
				}
			} else {
				ret.hash_value.insert(std::pair<std::string, mvt_value>(v1.string_value, v2));
			}
		}

		if (stringify_nested) {
			ret.string_value.append("}");
		}

		off--;  // so caller can increment
		return ret;
	}

	case 10: /* delta-encoded list */
	{
		ret.type = mvt_list;
		size_t len = property[off] >> 4;
		off++;

		if (off >= property.size()) {
			fprintf(stderr, "Not enough elements in list attribute\n");
			exit(EXIT_FAILURE);
		}
		size_t scaling = property[off];
		off++;

		if (scaling >= attribute_scalings.size()) {
			fprintf(stderr, "Reference to nonexistent attribute scaling\n");
			exit(EXIT_FAILURE);
		}

		if (stringify_nested) {
			ret.string_value = "[";
		}

		long here = 0;

		for (size_t i = 0; i < len; i++) {
			if (off >= property.size()) {
				fprintf(stderr, "Not enough elements in list attribute\n");
				exit(EXIT_FAILURE);
			}
			unsigned long increment = property[off];
			off++;

			double val;

			if (increment == 0) {
				val = NAN;
			} else {
				long inc = protozero::decode_zigzag64(increment - 1);
				here += inc;
				val = attribute_scalings[scaling].base + attribute_scalings[scaling].multiplier * (here + attribute_scalings[scaling].offset);
			}

			if (stringify_nested) {
				ret.string_value.append(milo::dtoa_milo(val));

				if (i + 1 < len) {
					ret.string_value.push_back(',');
				}
			} else {
				mvt_value v;
				v.type = mvt_double;
				v.numeric_value.double_value = val;

				ret.list_value.push_back(v);
			}
		}

		if (stringify_nested) {
			ret.string_value.append("]");
		}

		off--;  // so caller can increment
		return ret;
	}

	default:
		ret.type = mvt_string;
		ret.string_value = std::to_string(property[off]);
		return ret;
	}
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
		tv.numeric_value.null_value = 0;
	} else if (type == mvt_string) {
		tv.type = mvt_string;
		tv.string_value = s;
	} else {
		tv.type = mvt_hash; /* or list */
		tv.string_value = s;
	}

	return tv;
}

std::vector<mvt_geometry> to_feature(drawvec &geom, mvt_layer &layer) {
	std::vector<mvt_geometry> out;

	for (size_t i = 0; i < geom.size(); i++) {
		mvt_geometry g(geom[i].op, geom[i].x, geom[i].y, geom[i].elevations);

		if (geom[i].attributes.size() != 0) {
			json_pull *jp = json_begin_string(geom[i].attributes.c_str());
			json_object *jo = json_read_tree(jp);
			if (jo == NULL) {
				fprintf(stderr, "Internal error: failed to reconstruct JSON %s\n", geom[i].attributes.c_str());
				exit(EXIT_FAILURE);
			}
			tag_object_v3(layer, jo, g.attributes);
			json_free(jo);
			json_end(jp);
		}

		out.push_back(g);
	}

	return out;
}
