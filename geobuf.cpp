#include <stdio.h>
#include <string>
#include "mvt.hpp"
#include "serial.hpp"
#include "geobuf.hpp"
#include "geojson.hpp"
#include "protozero/varint.hpp"
#include "protozero/pbf_reader.hpp"
#include "protozero/pbf_writer.hpp"

serial_val readValue(protozero::pbf_reader &pbf, double e, std::vector<std::string> &keys) {
	serial_val sv;
	sv.type = mvt_null;
	sv.s = "null";

	while (pbf.next()) {
		switch (pbf.tag()) {
		case 1:
			sv.type = mvt_string;
			sv.s = pbf.get_string();
			break;

		case 2:
			sv.type = mvt_double;
			sv.s = std::to_string(pbf.get_double());
			break;

		case 3:
			sv.type = mvt_double;
			sv.s = std::to_string(pbf.get_uint64());
			break;

		case 4:
			sv.type = mvt_double;
			sv.s = std::to_string(-pbf.get_uint64());
			break;

		case 5:
			sv.type = mvt_bool;
			if (pbf.get_bool()) {
				sv.s = "true";
			} else {
				sv.s = "false";
			}
			break;

		case 6:
			sv.type = mvt_string;  // stringified JSON
			sv.s = pbf.get_string();
			break;

		default:
			pbf.skip();
		}
	}

	return sv;
}

drawvec readGeometry(protozero::pbf_reader &pbf, double e, std::vector<std::string> &keys, int &type) {
	std::vector<int> lengths;
	std::vector<long long> coords;

	while (pbf.next()) {
		switch (pbf.tag()) {
		case 1:
			type = pbf.get_enum();
			break;

		case 2: {
			auto pi = pbf.get_packed_sint32();
			for (auto it = pi.first; it != pi.second; ++it) {
				lengths.push_back(*it);
			}
			break;
		}

		case 3: {
			auto pi = pbf.get_packed_sint64();
			for (auto it = pi.first; it != pi.second; ++it) {
				coords.push_back(*it);
			}
			break;
		}

		case 4: {
			int type2;
			protozero::pbf_reader geometry_reader(pbf.get_message());
			drawvec dv2 = readGeometry(geometry_reader, e, keys, type2);
			break;
		}

		default:
			pbf.skip();
		}
	}

	type /= 2;
	drawvec dv;
	dv.push_back(draw(VT_MOVETO, 0, 0));
	dv.push_back(draw(VT_LINETO, 1000, 1000));
	return dv;
}

void readFeature(protozero::pbf_reader &pbf, double e, std::vector<std::string> &keys, struct serialization_state *sst, int layer, std::string layername) {
	drawvec dv;
	long long id = 0;
	bool has_id = false;
	std::vector<serial_val> values;
	std::vector<size_t> properties;

	while (pbf.next()) {
		switch (pbf.tag()) {
		case 1: {
			int type;
			protozero::pbf_reader geometry_reader(pbf.get_message());
			dv = readGeometry(geometry_reader, e, keys, type);
			break;
		}

		case 11:
			id = pbf.get_int64();
			has_id = true;
			break;

		case 12: {
			static bool warned = false;
			if (!warned) {
				fprintf(stderr, "Non-numeric feature IDs not supported");
				warned = true;
			}
			break;
		}

		case 13: {
			protozero::pbf_reader value_reader(pbf.get_message());
			values.push_back(readValue(value_reader, e, keys));
			break;
		}

		case 14: {
			auto pi = pbf.get_packed_uint32();
			for (auto it = pi.first; it != pi.second; ++it) {
				properties.push_back(*it);
			}
			break;
		}

		default:
			pbf.skip();
		}
	}

	serial_feature sf;
	sf.layer = layer;
	sf.layername = layername;
	sf.segment = 0; // single thread
	sf.has_id = has_id;
	sf.id = id;
	sf.has_tippecanoe_minzoom = false;
	sf.has_tippecanoe_maxzoom = false;
	sf.feature_minzoom = false;
	sf.seq = (*sst->layer_seq);
	sf.geometry = dv;

	for (size_t i = 0; i + 1 < properties.size(); i += 2) {
		if (properties[i] >= keys.size()) {
			fprintf(stderr, "Out of bounds key: %zu in %zu\n", properties[i], keys.size());
			exit(EXIT_FAILURE);
		}

		if (properties[i + 1] >= values.size()) {
			fprintf(stderr, "Out of bounds value: %zu in %zu\n", properties[i + 1], values.size());
			exit(EXIT_FAILURE);
		}

		sf.full_keys.push_back(keys[properties[i]]);
		sf.full_values.push_back(values[properties[i + 1]]);
	}

	sf.m = sf.full_values.size();

	serialize_feature(sst, sf);
}

void readFeatureCollection(protozero::pbf_reader &pbf, double e, std::vector<std::string> &keys, struct serialization_state *sst, int layer, std::string layername) {
	while (pbf.next()) {
		switch (pbf.tag()) {
		case 1: {
			protozero::pbf_reader feature_reader(pbf.get_message());
			readFeature(feature_reader, e, keys, sst, layer, layername);
			break;
		}

		default:
			pbf.skip();
		}
	}
}

void parse_geobuf(struct serialization_state *sst, std::string const &src, int layer, std::string layername) {
	protozero::pbf_reader pbf(src);

	long long dim = 2;
	double e = 10e6;
	std::vector<std::string> keys;

	while (pbf.next()) {
		switch (pbf.tag()) {
		case 1:
			keys.push_back(pbf.get_string());
			break;

		case 2:
			dim = pbf.get_int64();
			break;

		case 3:
			e = pow(10, pbf.get_int64());
			break;

		case 4: {
			protozero::pbf_reader feature_collection_reader(pbf.get_message());
			readFeatureCollection(feature_collection_reader, e, keys, sst, layer, layername);
			break;
		}

		case 5: {
			protozero::pbf_reader feature_reader(pbf.get_message());
			readFeature(feature_reader, e, keys, sst, layer, layername);
			break;
		}

		case 6: {
			int type;
			protozero::pbf_reader geometry_reader(pbf.get_message());
			drawvec dv = readGeometry(geometry_reader, e, keys, type);
			break;
		}

		default:
			pbf.skip();
		}
	}
}
