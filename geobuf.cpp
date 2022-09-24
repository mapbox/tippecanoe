#include <stdio.h>
#include <string>
#include <limits.h>
#include <pthread.h>
#include "mvt.hpp"
#include "serial.hpp"
#include "geobuf.hpp"
#include "geojson.hpp"
#include "projection.hpp"
#include "main.hpp"
#include "protozero/varint.hpp"
#include "protozero/pbf_reader.hpp"
#include "protozero/pbf_writer.hpp"
#include "milo/dtoa_milo.h"
#include "jsonpull/jsonpull.h"
#include "text.hpp"
#include "errors.hpp"

#define POINT 0
#define MULTIPOINT 1
#define LINESTRING 2
#define MULTILINESTRING 3
#define POLYGON 4
#define MULTIPOLYGON 5

struct queued_feature {
	protozero::pbf_reader pbf{};
	size_t dim = 0;
	double e = 0;
	std::vector<std::string> *keys = NULL;
	std::vector<struct serialization_state> *sst = NULL;
	int layer = 0;
	std::string layername = "";
};

static std::vector<queued_feature> feature_queue;

void ensureDim(size_t dim) {
	if (dim < 2) {
		fprintf(stderr, "Geometry has fewer than 2 dimensions: %zu\n", dim);
		exit(EXIT_IMPOSSIBLE);
	}
}

serial_val readValue(protozero::pbf_reader &pbf) {
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
			sv.s = milo::dtoa_milo(pbf.get_double());
			break;

		case 3:
			sv.type = mvt_double;
			sv.s = std::to_string(pbf.get_uint64());
			break;

		case 4:
			sv.type = mvt_double;
			sv.s = std::to_string(-(long long) pbf.get_uint64());
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

			if (sv.s == "null") {
				sv.type = mvt_null;
			}

			break;

		default:
			pbf.skip();
		}
	}

	return sv;
}

drawvec readPoint(std::vector<long long> &coords, size_t dim, double e) {
	ensureDim(dim);

	long long x, y;
	projection->project(coords[0] / e, coords[1] / e, 32, &x, &y);
	drawvec dv;
	dv.push_back(draw(VT_MOVETO, x, y));
	return dv;
}

drawvec readLinePart(std::vector<long long> &coords, size_t dim, double e, size_t start, size_t end, bool closed) {
	ensureDim(dim);

	drawvec dv;
	std::vector<long long> prev;
	std::vector<double> p;
	prev.resize(dim);
	p.resize(dim);

	for (size_t i = start; i + dim - 1 < end; i += dim) {
		if (i + dim - 1 >= coords.size()) {
			fprintf(stderr, "Internal error: line segment %zu vs %zu\n", i + dim - 1, coords.size());
			exit(EXIT_IMPOSSIBLE);
		}

		for (size_t d = 0; d < dim; d++) {
			prev[d] += coords[i + d];
			p[d] = prev[d] / e;
		}

		long long x, y;
		projection->project(p[0], p[1], 32, &x, &y);

		if (i == start) {
			dv.push_back(draw(VT_MOVETO, x, y));
		} else {
			dv.push_back(draw(VT_LINETO, x, y));
		}
	}

	if (closed && dv.size() > 0) {
		dv.push_back(draw(VT_LINETO, dv[0].x, dv[0].y));
	}

	return dv;
}

drawvec readLine(std::vector<long long> &coords, size_t dim, double e, bool closed) {
	return readLinePart(coords, dim, e, 0, coords.size(), closed);
}

drawvec readMultiLine(std::vector<long long> &coords, std::vector<int> &lengths, size_t dim, double e, bool closed) {
	if (lengths.size() == 0) {
		return readLinePart(coords, dim, e, 0, coords.size(), closed);
	}

	drawvec dv;
	size_t here = 0;
	for (size_t i = 0; i < lengths.size(); i++) {
		drawvec dv2 = readLinePart(coords, dim, e, here, here + lengths[i] * dim, closed);
		here += lengths[i] * dim;

		for (size_t j = 0; j < dv2.size(); j++) {
			dv.push_back(dv2[j]);
		}
	}

	return dv;
}

drawvec readMultiPolygon(std::vector<long long> &coords, std::vector<int> &lengths, size_t dim, double e) {
	ensureDim(dim);

	if (lengths.size() == 0) {
		return readLinePart(coords, dim, e, 0, coords.size(), true);
	}

	size_t polys = lengths[0];
	size_t n = 1;
	size_t here = 0;
	drawvec dv;

	for (size_t i = 0; i < polys; i++) {
		size_t rings = lengths[n++];

		for (size_t j = 0; j < rings; j++) {
			drawvec dv2 = readLinePart(coords, dim, e, here, here + lengths[n] * dim, true);
			here += lengths[n] * dim;
			n++;

			for (size_t k = 0; k < dv2.size(); k++) {
				dv.push_back(dv2[k]);
			}
		}

		dv.push_back(draw(VT_CLOSEPATH, 0, 0));	 // mark that the next ring is outer
	}

	return dv;
}

struct drawvec_type {
	drawvec dv{};
	int type = 0;
};

std::vector<drawvec_type> readGeometry(protozero::pbf_reader &pbf, size_t dim, double e, std::vector<std::string> &keys) {
	std::vector<drawvec_type> ret;
	std::vector<long long> coords;
	std::vector<int> lengths;
	int type = -1;

	while (pbf.next()) {
		switch (pbf.tag()) {
		case 1:
			type = pbf.get_enum();
			break;

		case 2: {
			auto pi = pbf.get_packed_uint32();
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
			protozero::pbf_reader geometry_reader(pbf.get_message());
			std::vector<drawvec_type> dv2 = readGeometry(geometry_reader, dim, e, keys);

			for (size_t i = 0; i < dv2.size(); i++) {
				ret.push_back(dv2[i]);
			}

			break;
		}

		default:
			pbf.skip();
		}
	}

	drawvec_type dv;
	if (type == POINT) {
		dv.dv = readPoint(coords, dim, e);
	} else if (type == MULTIPOINT) {
		dv.dv = readLine(coords, dim, e, false);
	} else if (type == LINESTRING) {
		dv.dv = readLine(coords, dim, e, false);
	} else if (type == POLYGON) {
		dv.dv = readMultiLine(coords, lengths, dim, e, true);
	} else if (type == MULTILINESTRING) {
		dv.dv = readMultiLine(coords, lengths, dim, e, false);
	} else if (type == MULTIPOLYGON) {
		dv.dv = readMultiPolygon(coords, lengths, dim, e);
	} else {
		// GeometryCollection
		return ret;
	}

	dv.type = type / 2 + 1;
	ret.push_back(dv);
	return ret;
}

void readFeature(protozero::pbf_reader &pbf, size_t dim, double e, std::vector<std::string> &keys, struct serialization_state *sst, int layer, std::string layername) {
	std::vector<drawvec_type> dv;
	long long id = 0;
	bool has_id = false;
	std::vector<serial_val> values;
	std::map<std::string, serial_val> other;

	std::vector<std::string> full_keys;
	std::vector<serial_val> full_values;

	while (pbf.next()) {
		switch (pbf.tag()) {
		case 1: {
			protozero::pbf_reader geometry_reader(pbf.get_message());
			std::vector<drawvec_type> dv2 = readGeometry(geometry_reader, dim, e, keys);
			for (size_t i = 0; i < dv2.size(); i++) {
				dv.push_back(dv2[i]);
			}
			break;
		}

		case 11: {
			static bool warned = false;
			if (!warned) {
				fprintf(stderr, "Non-numeric feature IDs not supported\n");
				warned = true;
			}
			pbf.skip();
			break;
		}

		case 12:
			has_id = true;
			id = pbf.get_sint64();
			if (id < 0) {
				static bool warned = false;
				if (!warned) {
					fprintf(stderr, "Out of range feature id %lld\n", id);
					warned = true;
				}
				has_id = false;
			}
			break;

		case 13: {
			protozero::pbf_reader value_reader(pbf.get_message());
			values.push_back(readValue(value_reader));
			break;
		}

		case 14: {
			std::vector<size_t> properties;
			auto pi = pbf.get_packed_uint32();
			for (auto it = pi.first; it != pi.second; ++it) {
				properties.push_back(*it);
			}

			for (size_t i = 0; i + 1 < properties.size(); i += 2) {
				if (properties[i] >= keys.size()) {
					fprintf(stderr, "Out of bounds key: %zu in %zu\n", properties[i], keys.size());
					exit(EXIT_IMPOSSIBLE);
				}

				if (properties[i + 1] >= values.size()) {
					fprintf(stderr, "Out of bounds value: %zu in %zu\n", properties[i + 1], values.size());
					exit(EXIT_IMPOSSIBLE);
				}

				full_keys.push_back(keys[properties[i]]);
				full_values.push_back(values[properties[i + 1]]);
			}

			values.clear();
			break;
		}

		case 15: {
			std::vector<size_t> misc;
			auto pi = pbf.get_packed_uint32();
			for (auto it = pi.first; it != pi.second; ++it) {
				misc.push_back(*it);
			}

			for (size_t i = 0; i + 1 < misc.size(); i += 2) {
				if (misc[i] >= keys.size()) {
					fprintf(stderr, "Out of bounds key: %zu in %zu\n", misc[i], keys.size());
					exit(EXIT_IMPOSSIBLE);
				}

				if (misc[i + 1] >= values.size()) {
					fprintf(stderr, "Out of bounds value: %zu in %zu\n", misc[i + 1], values.size());
					exit(EXIT_IMPOSSIBLE);
				}

				other.insert(std::pair<std::string, serial_val>(keys[misc[i]], values[misc[i + 1]]));
			}

			values.clear();
			break;
		}

		default:
			pbf.skip();
		}
	}

	for (size_t i = 0; i < dv.size(); i++) {
		serial_feature sf;

		sf.layer = layer;
		sf.layername = layername;
		sf.segment = sst->segment;
		sf.has_id = has_id;
		sf.id = id;
		sf.has_tippecanoe_minzoom = false;
		sf.has_tippecanoe_maxzoom = false;
		sf.feature_minzoom = false;
		sf.seq = *(sst->layer_seq);
		sf.geometry = dv[i].dv;
		sf.t = dv[i].type;
		sf.full_keys = full_keys;
		sf.full_values = full_values;

		auto tip = other.find("tippecanoe");
		if (tip != other.end()) {
			json_pull *jp = json_begin_string(tip->second.s.c_str());
			json_object *o = json_read_tree(jp);

			if (o != NULL) {
				json_object *min = json_hash_get(o, "minzoom");
				if (min != NULL && (min->type == JSON_NUMBER)) {
					sf.has_tippecanoe_minzoom = true;
					sf.tippecanoe_minzoom = integer_zoom(sst->fname, milo::dtoa_milo(min->value.number.number));
				}

				json_object *max = json_hash_get(o, "maxzoom");
				if (max != NULL && (max->type == JSON_NUMBER)) {
					sf.has_tippecanoe_maxzoom = true;
					sf.tippecanoe_maxzoom = integer_zoom(sst->fname, milo::dtoa_milo(max->value.number.number));
				}

				json_object *tlayer = json_hash_get(o, "layer");
				if (tlayer != NULL && (tlayer->type == JSON_STRING)) {
					sf.layername = tlayer->value.string.string;
				}
			}

			json_free(o);
			json_end(jp);
		}

		serialize_feature(sst, sf);
	}
}

struct queue_run_arg {
	size_t start;
	size_t end;
	size_t segment;

	queue_run_arg(size_t start1, size_t end1, size_t segment1)
	    : start(start1), end(end1), segment(segment1) {
	}
};

void *run_parse_feature(void *v) {
	struct queue_run_arg *qra = (struct queue_run_arg *) v;

	for (size_t i = qra->start; i < qra->end; i++) {
		struct queued_feature &qf = feature_queue[i];
		readFeature(qf.pbf, qf.dim, qf.e, *qf.keys, &(*qf.sst)[qra->segment], qf.layer, qf.layername);
	}

	return NULL;
}

void runQueue() {
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
		if (pthread_create(&pthreads[i], NULL, run_parse_feature, &qra[i]) != 0) {
			perror("pthread_create");
			exit(EXIT_PTHREAD);
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

void queueFeature(protozero::pbf_reader &pbf, size_t dim, double e, std::vector<std::string> &keys, std::vector<struct serialization_state> *sst, int layer, std::string layername) {
	struct queued_feature qf;
	qf.pbf = pbf;
	qf.dim = dim;
	qf.e = e;
	qf.keys = &keys;
	qf.sst = sst;
	qf.layer = layer;
	qf.layername = layername;

	feature_queue.push_back(qf);

	if (feature_queue.size() > CPUS * 500) {
		runQueue();
	}
}

void outBareGeometry(drawvec const &dv, int type, struct serialization_state *sst, int layer, std::string layername) {
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
	sf.t = type;

	serialize_feature(sst, sf);
}

void readFeatureCollection(protozero::pbf_reader &pbf, size_t dim, double e, std::vector<std::string> &keys, std::vector<struct serialization_state> *sst, int layer, std::string layername) {
	while (pbf.next()) {
		switch (pbf.tag()) {
		case 1: {
			protozero::pbf_reader feature_reader(pbf.get_message());
			queueFeature(feature_reader, dim, e, keys, sst, layer, layername);
			break;
		}

		default:
			pbf.skip();
		}
	}
}

void parse_geobuf(std::vector<struct serialization_state> *sst, const char *src, size_t len, int layer, std::string layername) {
	protozero::pbf_reader pbf(src, len);

	size_t dim = 2;
	double e = 1e6;
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
			readFeatureCollection(feature_collection_reader, dim, e, keys, sst, layer, layername);
			break;
		}

		case 5: {
			protozero::pbf_reader feature_reader(pbf.get_message());
			queueFeature(feature_reader, dim, e, keys, sst, layer, layername);
			break;
		}

		case 6: {
			protozero::pbf_reader geometry_reader(pbf.get_message());
			std::vector<drawvec_type> dv = readGeometry(geometry_reader, dim, e, keys);
			for (size_t i = 0; i < dv.size(); i++) {
				// Always on thread 0
				outBareGeometry(dv[i].dv, dv[i].type, &(*sst)[0], layer, layername);
			}
			break;
		}

		default:
			pbf.skip();
		}
	}

	runQueue();
}
