#include <string>
#include <vector>
#include <zlib.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/io/coded_stream.h>
#include "protobuf.hh"
#include "vector_tile.pb.h"

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

int dezig(unsigned n) {
	return (n >> 1) ^ (-(n & 1));
}

bool pb_decode(std::string &message, pb_tile &out) {
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// https://github.com/mapbox/mapnik-vector-tile/blob/master/examples/c%2B%2B/tileinfo.cpp
	mapnik::vector::tile tile;
	out.layers.clear();

	if (is_compressed(message)) {
		std::string uncompressed;
		decompress(message, uncompressed);
		google::protobuf::io::ArrayInputStream stream(uncompressed.c_str(), uncompressed.length());
		google::protobuf::io::CodedInputStream codedstream(&stream);
		codedstream.SetTotalBytesLimit(10 * 67108864, 5 * 67108864);
		if (!tile.ParseFromCodedStream(&codedstream)) {
			return false;
		}
	} else if (!tile.ParseFromString(message)) {
		return false;
	}

	for (int l = 0; l < tile.layers_size(); l++) {
		mapnik::vector::tile_layer layer = tile.layers(l);
		pb_layer pbl;
		pbl.extent = layer.extent();
		pbl.name = layer.name();

		for (size_t i = 0; i < layer.keys_size(); i++) {
			pbl.keys.push_back(layer.keys(i));
		}

		for (size_t i = 0; i < layer.values_size(); i++) {
			mapnik::vector::tile_value const &val = layer.values(i);
			pb_value pbv;

			if (val.has_string_value()) {
				pbv.type = pb_string;
				pbv.string_value = val.string_value();
			} else if (val.has_float_value()) {
				pbv.type = pb_float;
				pbv.numeric_value.float_value = val.float_value();
			} else if (val.has_double_value()) {
				pbv.type = pb_double;
				pbv.numeric_value.double_value = val.double_value();
			} else if (val.has_int_value()) {
				pbv.type = pb_int;
				pbv.numeric_value.int_value = val.int_value();
			} else if (val.has_uint_value()) {
				pbv.type = pb_uint;
				pbv.numeric_value.uint_value = val.uint_value();
			} else if (val.has_sint_value()) {
				pbv.type = pb_sint;
				pbv.numeric_value.sint_value = val.sint_value();
			} else if (val.has_bool_value()) {
				pbv.type = pb_bool;
				pbv.numeric_value.bool_value = val.bool_value();
			}

			pbl.values.push_back(pbv);
		}

		for (int f = 0; f < layer.features_size(); f++) {
			mapnik::vector::tile_feature feat = layer.features(f);
			pb_feature pbf;
			pbf.type = feat.type();

			for (size_t i = 0; i < feat.tags_size(); i++) {
				pbf.tags.push_back(feat.tags(i));
			}

			long long px = 0, py = 0;
			for (size_t g = 0; g < feat.geometry_size(); g++) {
				uint32_t geom = feat.geometry(g);
				uint32_t op = geom & 7;
				uint32_t count = geom >> 3;

				if (op == pb_moveto || op == pb_lineto) {
					for (size_t k = 0; k < count; k++) {
						px += dezig(feat.geometry(g + 1));
						py += dezig(feat.geometry(g + 2));
						g += 2;

						pbf.geometry.push_back(pb_geometry(op, px, py));
					}
				} else {
					pbf.geometry.push_back(pb_geometry(op, 0, 0));
				}
			}

			pbl.features.push_back(pbf);
		}

		out.layers.push_back(pbl);
	}

	return true;
}
