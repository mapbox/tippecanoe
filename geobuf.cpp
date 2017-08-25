#include <stdio.h>
#include <string>
#include "serial.hpp"
#include "geobuf.hpp"
#include "protozero/varint.hpp"
#include "protozero/pbf_reader.hpp"
#include "protozero/pbf_writer.hpp"

serial_val readValue(protozero::pbf_reader &pbf, double e, std::vector<std::string> &keys) {
	return serial_val();
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
	return drawvec();
}

void readFeature(protozero::pbf_reader &pbf, double e, std::vector<std::string> &keys) {
	drawvec dv;
	long long id = -1;
	std::vector<serial_val> values;
	std::vector<int> properties;

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
			break;

		case 12:
			fprintf(stderr, "Non-numeric feature IDs not supported");
			break;

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
}

void readFeatureCollection(protozero::pbf_reader &pbf, double e, std::vector<std::string> &keys) {
	while (pbf.next()) {
		switch (pbf.tag()) {
		case 1: {
			protozero::pbf_reader feature_reader(pbf.get_message());
			readFeature(feature_reader, e, keys);
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
			readFeatureCollection(feature_collection_reader, e, keys);
			break;
		}

		case 5: {
			protozero::pbf_reader feature_reader(pbf.get_message());
			readFeature(feature_reader, e, keys);
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
