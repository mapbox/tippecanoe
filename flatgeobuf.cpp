#include <stdio.h>
#include "serial.hpp"
#include <iostream>
#include "projection.hpp"
#include "flatgeobuf/feature_generated.h"
#include "flatgeobuf/header_generated.h"
#include "milo/dtoa_milo.h"
#include "main.hpp"

static constexpr uint8_t magicbytes[8] = { 0x66, 0x67, 0x62, 0x03, 0x66, 0x67, 0x62, 0x01 };

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

drawvec readPoints(const FlatGeobuf::Geometry *geometry) {
	auto xy = geometry->xy();
	drawvec dv;

	for (unsigned int i = 0; i < xy->size(); i+=2) {
		long long x, y;
		projection->project(xy->Get(i), xy->Get(i+1), 32, &x, &y);
		dv.push_back(draw(VT_MOVETO, x, y));
	}
	return dv;
}

drawvec readLinePart(const FlatGeobuf::Geometry *geometry) {
	auto xy = geometry->xy();
	auto ends = geometry->ends();
	size_t current_end = 0;
	drawvec dv;

	for (unsigned int i = 0; i < xy->size(); i+=2) {
		long long x, y;
		projection->project(xy->Get(i), xy->Get(i+1), 32, &x, &y);
		if (i == 0 || (ends != NULL && current_end < ends->size() && i == ends->Get(current_end)*2)) {
			dv.push_back(draw(VT_MOVETO, x, y));
			if (i > 0) current_end++;
		} else {
			dv.push_back(draw(VT_LINETO, x, y));
		}
	}
	return dv;
}

drawvec readGeometry(const FlatGeobuf::Geometry *geometry, FlatGeobuf::GeometryType h_geometry_type) {
	if (h_geometry_type == FlatGeobuf::GeometryType::Point) {
		return readPoints(geometry);
	} if (h_geometry_type == FlatGeobuf::GeometryType::MultiPoint) {
		return readPoints(geometry);	
	} if (h_geometry_type == FlatGeobuf::GeometryType::LineString) {
		return readLinePart(geometry);
	} else if (h_geometry_type == FlatGeobuf::GeometryType::MultiLineString) {
		return readLinePart(geometry);
	} if (h_geometry_type == FlatGeobuf::GeometryType::Polygon) {
		return readLinePart(geometry);
	} else if (h_geometry_type == FlatGeobuf::GeometryType::MultiPolygon) { 
	// if it is a GeometryCollection, parse Parts, ignore XY
		drawvec dv;
		for (size_t part = 0; part < geometry->parts()->size(); part++) {
			drawvec dv2 = readLinePart(geometry->parts()->Get(part));
			for (size_t k = 0; k < dv2.size(); k++) {
				dv.push_back(dv2[k]);
			}
			dv.push_back(draw(VT_CLOSEPATH, 0, 0));
		}
		return dv;
	} else {
		fprintf(stderr, "flatgeobuf has unsupported geometry type %u\n", (unsigned int)h_geometry_type);
		exit(EXIT_FAILURE);
	}
}

void readFeature(const FlatGeobuf::Feature *feature, FlatGeobuf::GeometryType h_geometry_type, const std::vector<std::string> &h_column_names, const std::vector<FlatGeobuf::ColumnType> &h_column_types, struct serialization_state *sst, int layer, std::string layername) {
	drawvec dv = readGeometry(feature->geometry(), h_geometry_type);

	int drawvec_type = -1;

	switch (h_geometry_type) {
		case FlatGeobuf::GeometryType::Point :
		case FlatGeobuf::GeometryType::MultiPoint :
			drawvec_type = 1;
			break;
		case FlatGeobuf::GeometryType::LineString :
		case FlatGeobuf::GeometryType::MultiLineString :
			drawvec_type = 2;
			break;
		case FlatGeobuf::GeometryType::Polygon :
		case FlatGeobuf::GeometryType::MultiPolygon :
			drawvec_type = 3;
			break;
		case FlatGeobuf::GeometryType::Unknown :
		case FlatGeobuf::GeometryType::GeometryCollection :
		default:
			fprintf(stderr, "flatgeobuf has unsupported geometry type %u\n", (unsigned int)h_geometry_type);
			exit(EXIT_FAILURE);
	}

	serial_feature sf;

	sf.layer = layer;
	sf.layername = layername;
	sf.segment = sst->segment;
	sf.has_id = false;
	sf.has_tippecanoe_minzoom = false;
	sf.has_tippecanoe_maxzoom = false;
	sf.feature_minzoom = false;
	sf.seq = (*sst->layer_seq);
	sf.geometry = dv;
	sf.t = drawvec_type;

	std::vector<std::string> full_keys;
	std::vector<serial_val> full_values;

	// assume tabular schema with columns in header
	size_t p_pos = 0;
	while (p_pos < feature->properties()->size()) {
		uint16_t col_idx;
		memcpy(&col_idx, feature->properties()->data() + p_pos, sizeof(col_idx));

		FlatGeobuf::ColumnType col_type = h_column_types[col_idx];

		serial_val sv;
		if (col_type == FlatGeobuf::ColumnType::Int) {
			sv.type = mvt_double; // this is quirky
			int32_t int_val;
			memcpy(&int_val, feature->properties()->data() + p_pos + sizeof(uint16_t), sizeof(int_val));
			sv.s = std::to_string(int_val);
			p_pos += sizeof(uint16_t) + sizeof(int_val);
		} else if (col_type == FlatGeobuf::ColumnType::Long) {
			sv.type = mvt_double; // this is quirky
			uint64_t long_val;
			memcpy(&long_val, feature->properties()->data() + p_pos + sizeof(uint16_t), sizeof(long_val));
			sv.s = std::to_string(long_val);
			p_pos += sizeof(uint16_t) + sizeof(long_val);
		} else if (col_type == FlatGeobuf::ColumnType::Double) {
			sv.type = mvt_double;
			double double_val;
			memcpy(&double_val, feature->properties()->data() + p_pos + sizeof(uint16_t), sizeof(double_val));
			sv.s = milo::dtoa_milo(double_val);
			p_pos += sizeof(uint16_t) + sizeof(double_val);
		} else if (col_type == FlatGeobuf::ColumnType::String) {
			sv.type = mvt_string;
			uint32_t str_len;
			memcpy(&str_len, feature->properties()->data() + p_pos + sizeof(uint16_t), sizeof(str_len));
			std::string s{reinterpret_cast<const char*>(feature->properties()->data() + p_pos + sizeof(uint16_t) + sizeof(uint32_t)), str_len};
			sv.s = s;
			p_pos += sizeof(uint16_t) + sizeof(uint32_t) + str_len;
		} else {
			fprintf(stderr, "flatgeobuf has unsupported column type %u\n", (unsigned int)col_type);
			exit(EXIT_FAILURE);
		}
		full_keys.push_back(h_column_names[col_idx]);
		full_values.push_back(sv);
	}

	sf.full_keys = full_keys;
	sf.full_values = full_values;

	serialize_feature(sst, sf);
}

struct queued_feature {
	const FlatGeobuf::Feature *feature = NULL;
	FlatGeobuf::GeometryType h_geometry_type = FlatGeobuf::GeometryType::Unknown;
	const std::vector<std::string> *h_column_names = NULL;
	const std::vector<FlatGeobuf::ColumnType> *h_column_types = NULL;
	std::vector<struct serialization_state> *sst = NULL;
	int layer = 0;
	std::string layername = "";
};

static std::vector<queued_feature> feature_queue;

struct queue_run_arg {
	size_t start;
	size_t end;
	size_t segment;

	queue_run_arg(size_t start1, size_t end1, size_t segment1)
	    : start(start1), end(end1), segment(segment1) {
	}
};

void *fgb_run_parse_feature(void *v) {
	struct queue_run_arg *qra = (struct queue_run_arg *) v;

	for (size_t i = qra->start; i < qra->end; i++) {
		struct queued_feature &qf = feature_queue[i];
		readFeature(qf.feature, qf.h_geometry_type, *qf.h_column_names, *qf.h_column_types, &(*qf.sst)[qra->segment], qf.layer, qf.layername);
	}

	return NULL;
}

void fgbRunQueue() {
	if (feature_queue.size() == 0) {
		return;
	}

	std::vector<struct queue_run_arg> qra;

	std::vector<pthread_t> pthreads;
	pthreads.resize(CPUS);

	for (size_t i = 0; i < CPUS; i++) {
		*((*(feature_queue[0].sst))[i].layer_seq) = *((*(feature_queue[0].sst))[0].layer_seq) + feature_queue.size() * i / CPUS;

		qra.push_back(queue_run_arg(
			feature_queue.size() * i / CPUS,
			feature_queue.size() * (i + 1) / CPUS,
			i));
	}

	for (size_t i = 0; i < CPUS; i++) {
		if (pthread_create(&pthreads[i], NULL, fgb_run_parse_feature, &qra[i]) != 0) {
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
	}

	for (size_t i = 0; i < CPUS; i++) {
		void *retval;

		if (pthread_join(pthreads[i], &retval) != 0) {
			perror("pthread_join");
		}
	}

	// Lack of atomicity is OK, since we are single-threaded again here
	long long was = *((*(feature_queue[0].sst))[CPUS - 1].layer_seq);
	*((*(feature_queue[0].sst))[0].layer_seq) = was;
	feature_queue.clear();
}

void queueFeature(const FlatGeobuf::Feature *feature, FlatGeobuf::GeometryType h_geometry_type, const std::vector<std::string> &h_column_names, const std::vector<FlatGeobuf::ColumnType> &h_column_types, std::vector<struct serialization_state> *sst, int layer, std::string layername) {
	struct queued_feature qf;
	qf.feature = feature;
	qf.h_geometry_type = h_geometry_type;
	qf.h_column_names = &h_column_names;
	qf.h_column_types = &h_column_types;
	qf.sst = sst;
	qf.layer = layer;
	qf.layername = layername;

	feature_queue.push_back(qf);

	if (feature_queue.size() > CPUS * 500) {
		fgbRunQueue();
	}
}

void parse_flatgeobuf(std::vector<struct serialization_state> *sst, const char *src, size_t len, int layer, std::string layername) {
	auto header_size = flatbuffers::GetPrefixedSize((const uint8_t *)src + sizeof(magicbytes));

	flatbuffers::Verifier v((const uint8_t *)src+sizeof(magicbytes),header_size+sizeof(uint32_t));
	const auto ok = FlatGeobuf::VerifySizePrefixedHeaderBuffer(v);
	if (!ok) {
		fprintf(stderr, "flatgeobuf header header verification failed\n");
		exit(EXIT_FAILURE);
	}

	auto header = FlatGeobuf::GetSizePrefixedHeader(src + sizeof(magicbytes));
	auto features_count = header->features_count();
	auto node_size = header->index_node_size();

	std::vector<std::string> h_column_names;
	std::vector<FlatGeobuf::ColumnType> h_column_types;

	if (header->columns() != NULL) {
		for (size_t i = 0; i < header->columns()->size(); i++) {
			h_column_names.push_back(header->columns()->Get(i)->name()->c_str());
			h_column_types.push_back(header->columns()->Get(i)->type());
		}
	}

	auto h_geometry_type = header->geometry_type();

	int index_size = 0;
	if (node_size > 0) {
		index_size = PackedRTreeSize(features_count,node_size);
	}
	const char* start = src + sizeof(magicbytes) + sizeof(uint32_t) + header_size + index_size;

	while (start < src + len) {
		auto feature_size = flatbuffers::GetPrefixedSize((const uint8_t *)start);

		flatbuffers::Verifier v2((const uint8_t *)start,feature_size+sizeof(uint32_t));
		const auto ok2 = FlatGeobuf::VerifySizePrefixedFeatureBuffer(v2);
		if (!ok2) {
			fprintf(stderr, "flatgeobuf feature buffer verification failed\n");
			exit(EXIT_FAILURE);
		}

		auto feature = FlatGeobuf::GetSizePrefixedFeature(start);

		queueFeature(feature, h_geometry_type, h_column_names, h_column_types, sst, layer, layername);

		start += sizeof(uint32_t) + feature_size;
	}

	fgbRunQueue();
}