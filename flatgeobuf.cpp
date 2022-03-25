#include <stdio.h>
#include "serial.hpp"
#include <iostream>
#include "projection.hpp"
#include "flatgeobuf/feature_generated.h"
#include "flatgeobuf/header_generated.h"

using namespace std;

struct NodeItem {
    double minX;
    double minY;
    double maxX;
    double maxY;
    uint64_t offset;
};

// copied from https://github.com/flatgeobuf/flatgeobuf/blob/master/src/cpp/packedrtree.cpp#L365
uint64_t PackedRTreeSize(const uint64_t numItems, const uint16_t nodeSize)
{
    if (nodeSize < 2)
        throw std::invalid_argument("Node size must be at least 2");
    if (numItems == 0)
        throw std::invalid_argument("Number of items must be greater than 0");
    const uint16_t nodeSizeMin = std::min(std::max(nodeSize, static_cast<uint16_t>(2)), static_cast<uint16_t>(65535));
    // limit so that resulting size in bytes can be represented by uint64_t
    if (numItems > static_cast<uint64_t>(1) << 56)
        throw std::overflow_error("Number of items must be less than 2^56");
    uint64_t n = numItems;
    uint64_t numNodes = n;
    do {
        n = (n + nodeSizeMin - 1) / nodeSizeMin;
        numNodes += n;
    } while (n != 1);
    return numNodes * sizeof(NodeItem);
}

drawvec readGeometry(const FlatGeobuf::Geometry *geometry, FlatGeobuf::GeometryType geometry_type) {
	// if it is a GeometryCollection, parse Parts, ignore XY
	auto xy = geometry->xy();
	auto ends = geometry->ends();
	size_t current_end = 0;

	drawvec dv;
	for (unsigned int i = 0; i < xy->size(); i+=2) {
		long long x, y;
		projection->project(xy->Get(i), xy->Get(i+1), 32, &x, &y);
		if (i == 0 || (ends != nullptr && current_end < ends->size() && i == ends->Get(current_end))) {
			dv.push_back(draw(VT_MOVETO, x, y));
			if (i > 0) current_end++;
		} else {
			dv.push_back(draw(VT_LINETO, x, y));
		}
	}

	return dv;
}

void parse_flatgeobuf(std::vector<struct serialization_state> *sst, const char *src, int layer, std::string layername) {
	auto header_size = flatbuffers::GetPrefixedSize((const uint8_t *)src + 8);
	auto header = FlatGeobuf::GetSizePrefixedHeader(src + 8);
	auto features_count = header->features_count();
	auto node_size = header->index_node_size();

	auto index_size = PackedRTreeSize(features_count,node_size);

	auto h_geometry_type = header->geometry_type();

	switch (h_geometry_type) {
		case FlatGeobuf::GeometryType::Unknown :
		case FlatGeobuf::GeometryType::Point :
		case FlatGeobuf::GeometryType::LineString :
		case FlatGeobuf::GeometryType::Polygon :
		case FlatGeobuf::GeometryType::MultiPoint :
		case FlatGeobuf::GeometryType::MultiLineString :
		case FlatGeobuf::GeometryType::MultiPolygon :
		case FlatGeobuf::GeometryType::GeometryCollection :
			break;
		default:
			fprintf(stderr, "flatgeobuf has unsupported geometry type\n");
			exit(EXIT_FAILURE);
	}

	const char* start = src + 8 + 4 + header_size + index_size;
	for (size_t i = 0; i < features_count; i++) {
		
		serial_feature sf;

		auto my_sst = &(*sst)[0];

		auto feature_size = flatbuffers::GetPrefixedSize((const uint8_t *)start);
		auto feature = FlatGeobuf::GetSizePrefixedFeature(start);
		drawvec dv = readGeometry(feature->geometry(), h_geometry_type);

		sf.layer = layer;
		sf.layername = layername;
		sf.segment = my_sst->segment;
		sf.has_id = false;
		sf.has_tippecanoe_minzoom = false;
		sf.has_tippecanoe_maxzoom = false;
		sf.feature_minzoom = false;
		sf.seq = (*my_sst->layer_seq);
		sf.geometry = dv;
		sf.t = 4;

		serialize_feature(my_sst, sf);
		start += 4 + feature_size;
	}
}