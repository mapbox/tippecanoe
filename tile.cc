#include <iostream>
#include <fstream>
#include <string>
#include <stack>
#include <vector>
#include <map>
#include <algorithm>
#include <stdio.h>
#include <unistd.h>
#include <zlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <sqlite3.h>
#include "vector_tile.pb.h"
#include "geometry.hh"

extern "C" {
	#include "tile.h"
	#include "pool.h"
	#include "clip.h"
	#include "mbtiles.h"
	#include "projection.h"
}

#define CMD_BITS 3
#define MIN_DETAIL 7

// https://github.com/mapbox/mapnik-vector-tile/blob/master/src/vector_tile_compression.hpp
static inline int compress(std::string const& input, std::string& output) {
	z_stream deflate_s;
	deflate_s.zalloc = Z_NULL;
	deflate_s.zfree = Z_NULL;
	deflate_s.opaque = Z_NULL;
	deflate_s.avail_in = 0;
	deflate_s.next_in = Z_NULL;
	deflateInit(&deflate_s, Z_DEFAULT_COMPRESSION);
	deflate_s.next_in = (Bytef *)input.data();
	deflate_s.avail_in = input.size();
	size_t length = 0;
	do {
		size_t increase = input.size() / 2 + 1024;
		output.resize(length + increase);
		deflate_s.avail_out = increase;
		deflate_s.next_out = (Bytef *)(output.data() + length);
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

int to_feature(drawvec &geom, mapnik::vector::tile_feature *feature) {
	int px = 0, py = 0;
	int cmd_idx = -1;
	int cmd = -1;
	int length = 0;
	int drew = 0;
	int i;

	int n = geom.size();
	for (i = 0; i < n; i++) {
		int op = geom[i].op;

		if (op != cmd) {
			if (cmd_idx >= 0) {
				if (feature != NULL) {
					feature->set_geometry(cmd_idx, (length << CMD_BITS) | (cmd & ((1 << CMD_BITS) - 1)));
				}
			}

			cmd = op;
			length = 0;

			if (feature != NULL) {
				cmd_idx = feature->geometry_size();
				feature->add_geometry(0);
			}
		}

		if (op == VT_MOVETO || op == VT_LINETO) {
			long long wwx = geom[i].x;
			long long wwy = geom[i].y;

			int dx = wwx - px;
			int dy = wwy - py;

			if (feature != NULL) {
				feature->add_geometry((dx << 1) ^ (dx >> 31));
				feature->add_geometry((dy << 1) ^ (dy >> 31));
			}

			px = wwx;
			py = wwy;
			length++;

			if (op == VT_LINETO && (dx != 0 || dy != 0)) {
				drew = 1;
			}
		} else if (op == VT_CLOSEPATH) {
			length++;
		} else {
			fprintf(stderr, "\nInternal error: corrupted geometry\n");
			exit(EXIT_FAILURE);
		}
	}

	if (cmd_idx >= 0) {
		if (feature != NULL) {
			feature->set_geometry(cmd_idx, (length << CMD_BITS) | (cmd & ((1 << CMD_BITS) - 1)));
		}
	}

	return drew;
}

int coalindexcmp(const struct coalesce *c1, const struct coalesce *c2);

struct coalesce {
	int type;
	drawvec geom;
	std::vector<int> meta;
	unsigned long long index;
	unsigned long long index2;
	char *metasrc;
	bool coalesced;

	bool operator< (const coalesce &o) const {
		int cmp = coalindexcmp(this, &o);
		if (cmp < 0) {
			return true;
		} else {
			return false;
		}
	}
};

int coalcmp(const void *v1, const void *v2) {
	const struct coalesce *c1 = (const struct coalesce *) v1;
	const struct coalesce *c2 = (const struct coalesce *) v2;

	int cmp = c1->type - c2->type;
	if (cmp != 0) {
		return cmp;
	}

	unsigned i;
	for (i = 0; i < c1->meta.size() && i < c2->meta.size(); i++) {
		cmp = c1->meta[i] - c2->meta[i];

		if (cmp != 0) {
			return cmp;
		}
	}

	if (c1->meta.size() < c2->meta.size()) {
		return -1;
	} else if (c1->meta.size() > c2->meta.size()) {
		return 1;
	} else {
		return 0;
	}
}

int coalindexcmp(const struct coalesce *c1, const struct coalesce *c2) {
	int cmp = coalcmp((const void *) c1, (const void *) c2);

	if (cmp == 0) {
		if (c1->index < c2->index) {
			return -1;
		} else if (c1->index > c2->index) {
			return 1;
		}

		if (c1->index2 > c2->index2) {
			return -1;
		} else if (c1->index2 < c2->index2) {
			return 1;
		}
	}

	return cmp;
}

void decode_meta(char **meta, struct pool *keys, struct pool *values, struct pool *file_keys, std::vector<int> *intmeta, char *only) {
	int m;
	deserialize_int(meta, &m);

	int i;
	for (i = 0; i < m; i++) {
		int t;
		deserialize_int(meta, &t);
		struct pool_val *key = deserialize_string(meta, keys, VT_STRING);

		if (only != NULL && (strcmp(key->s, only) != 0)) {
			deserialize_int(meta, &t);
			*meta += t;
		} else {
			struct pool_val *value = deserialize_string(meta, values, t);

			intmeta->push_back(key->n);
			intmeta->push_back(value->n);

			if (!is_pooled(file_keys, key->s, t)) {
				// Dup to retain after munmap
				pool(file_keys, strdup(key->s), t);
			}
		}
	}
}

mapnik::vector::tile create_tile(char *layername, int line_detail, std::vector<coalesce> &features, long long *count, struct pool *keys, struct pool *values) {
	mapnik::vector::tile tile;
	mapnik::vector::tile_layer *layer = tile.add_layers();

	layer->set_name(layername);
	layer->set_version(1);
	layer->set_extent(1 << line_detail);

	unsigned x;
	for (x = 0; x < features.size(); x++) {
		if (features[x].type == VT_LINE || features[x].type == VT_POLYGON) {
			features[x].geom = remove_noop(features[x].geom, features[x].type);
		}

		mapnik::vector::tile_feature *feature = layer->add_features();

		if (features[x].type == VT_POINT) {
			feature->set_type(mapnik::vector::tile::Point);
		} else if (features[x].type == VT_LINE) {
			feature->set_type(mapnik::vector::tile::LineString);
		} else if (features[x].type == VT_POLYGON) {
			feature->set_type(mapnik::vector::tile::Polygon);
		} else {
			feature->set_type(mapnik::vector::tile::Unknown);
		}

		to_feature(features[x].geom, feature);
		*count += features[x].geom.size();

		unsigned y;
		for (y = 0; y < features[x].meta.size(); y++) {
			feature->add_tags(features[x].meta[y]);
		}
	}

	struct pool_val *pv;
	for (pv = keys->head; pv != NULL; pv = pv->next) {
		layer->add_keys(pv->s, strlen(pv->s));
	}
	for (pv = values->head; pv != NULL; pv = pv->next) {
		mapnik::vector::tile_value *tv = layer->add_values();

		if (pv->type == VT_NUMBER) {
			tv->set_double_value(atof(pv->s));
		} else {
			tv->set_string_value(pv->s);
		}
	}

	return tile;
}

struct sll {
	char *name;	
	long long val;

	bool operator< (const sll &o) const {
		if (this->val < o.val) {
			return true;
		} else {
			return false;
		}
	}

	sll(char *name, long long val) {
		this->name = name;
		this->val = val;
	}
};

void evaluate(std::vector<coalesce> &features, char *metabase, struct pool *file_keys, char *layername, int line_detail, long long orig) {
	std::vector<sll> options;

	struct pool_val *pv;
	for (pv = file_keys->head; pv != NULL; pv = pv->next) {
		struct pool keys, values;
		pool_init(&keys, 0);
		pool_init(&values, 0);
		long long count = 0;

		for (unsigned i = 0; i < features.size(); i++) {
			char *meta = features[i].metasrc;

			features[i].meta.resize(0);
			decode_meta(&meta, &keys, &values, file_keys, &features[i].meta, pv->s);
		}

		std::vector<coalesce> empty;
		mapnik::vector::tile tile = create_tile(layername, line_detail, empty, &count, &keys, &values);

		std::string s;
		std::string compressed;

		tile.SerializeToString(&s);
		compress(s, compressed);

		options.push_back(sll(pv->s, compressed.size()));

		pool_free(&values);
		pool_free(&keys);
	}

	std::sort(options.begin(), options.end());
	for (unsigned i = 0; i < options.size(); i++) {
		if (options[i].val > 1024) {
			fprintf(stderr, "using -x %s would save about %lld, for a tile size of of %lld\n", options[i].name, options[i].val, orig - options[i].val);
		}
	}

	struct pool keys, values;
	pool_init(&keys, 0);
	pool_init(&values, 0);
	long long count = 0;

	std::vector<coalesce> empty;
	mapnik::vector::tile tile = create_tile(layername, line_detail, features, &count, &keys, &values);

	std::string s;
	std::string compressed;

	tile.SerializeToString(&s);
	compress(s, compressed);
	fprintf(stderr, "geometry alone (-X) would be %lld\n", (long long) compressed.size());

	pool_free(&values);
	pool_free(&keys);
}

long long write_tile(struct index *start, struct index *end, char *metabase, unsigned *file_bbox, int z, unsigned tx, unsigned ty, int detail, int basezoom, struct pool *file_keys, char *layername, sqlite3 *outdb, double droprate, int buffer) {
	int line_detail;
	static bool evaluated = false;

	for (line_detail = detail; line_detail >= MIN_DETAIL; line_detail--) {
		GOOGLE_PROTOBUF_VERIFY_VERSION;

		struct pool keys, values;
		pool_init(&keys, 0);
		pool_init(&values, 0);
		std::map<long long, int> dup;

		double interval = 1;
		double seq = 0;
		long long count = 0;
		//long long along = 0;
		double accum_area = 0;

		if (z < basezoom) {
			interval = exp(log(droprate) * (basezoom - z));
		}

		std::vector<coalesce> features;

		struct index *i;
		for (i = start; i < end; i++) {
			if (z > i->maxzoom) {
				continue;
			}

			if (dup.count(i->fpos) != 0) {
				continue;
			}
			dup.insert(std::pair<long long, int>(i->fpos, 1));

			int t = i->type;
			if (t == VT_POINT) {
				seq++;

				if (seq >= 0) {
					seq -= interval;
				} else {
					continue;
				}
			}

			char *meta = metabase + i->fpos;
			drawvec geom = decode_geometry(&meta, z, tx, ty, line_detail);

			bool reduced = false;
			if (t == VT_POLYGON) {
				geom = reduce_tiny_poly(geom, z, line_detail, &reduced, &accum_area);
			}

			if (t == VT_LINE) {
				geom = clip_lines(geom, z, line_detail, buffer);
			}

			if (t == VT_POLYGON) {
				geom = clip_poly(geom, z, line_detail, buffer);
			}

			if (t == VT_LINE || t == VT_POLYGON) {
				if (!reduced) {
					geom = simplify_lines(geom, z, line_detail);
				}
			}

#if 0
			if (t == VT_LINE && z != basezoom) {
				geom = shrink_lines(geom, z, line_detail, basezoom, &along);
			}
#endif

			if (t == VT_LINE) {
				geom = reorder_lines(geom);
			}

			to_tile_scale(geom, z, line_detail);

			if (t == VT_POINT || to_feature(geom, NULL)) {
				struct coalesce c;

				c.type = t;
				if (geom.size() > 0) {
					c.index = encode(geom[0].x, geom[0].y);
					c.index2 = encode(geom[geom.size() - 1].x, geom[geom.size() - 1].y);

					// Anything numbered below the start of the line
					// can't possibly be the next feature.
					// We want lowest-but-not-under.
					if (c.index2 < c.index) {
						c.index2 = ~0LL;
					}
				} else {
					c.index = i->index;
					c.index2 = i->index;
				}
				c.geom = geom;
				c.metasrc = meta;
				c.coalesced = false;

				decode_meta(&meta, &keys, &values, file_keys, &c.meta, NULL);
				features.push_back(c);
			}
		}

		std::sort(features.begin(), features.end());

		std::vector<coalesce> out;
		unsigned x;
		for (x = 0; x < features.size(); x++) {
			unsigned y = out.size() - 1;

			if (out.size() > 0 && coalcmp(&features[x], &out[y]) < 0) {
				fprintf(stderr, "\nfeature out of order\n");
			}

			if (out.size() > 0 && out[y].geom.size() + features[x].geom.size() < 20000 && coalcmp(&features[x], &out[y]) == 0 && features[x].type != VT_POINT) {
				unsigned z;
				for (z = 0; z < features[x].geom.size(); z++) {
					out[y].geom.push_back(features[x].geom[z]);
				}
				out[y].coalesced = true;
			} else {
				out.push_back(features[x]);
			}
		}
		features = out;

		for (x = 0; x < features.size(); x++) {
			if (features[x].coalesced && features[x].type == VT_LINE) {
				features[x].geom = remove_noop(features[x].geom, features[x].type);
				features[x].geom = simplify_lines(features[x].geom, 32, 0);
			}
		}

		mapnik::vector::tile tile = create_tile(layername, line_detail, features, &count, &keys, &values);

		pool_free(&keys);
		pool_free(&values);

		std::string s;
		std::string compressed;

		tile.SerializeToString(&s);
		compress(s, compressed);

		if (compressed.size() > 500000) {
			fprintf(stderr, "tile %d/%u/%u size is %lld with detail %d, >500000    \n", z, tx, ty, (long long) compressed.size(), line_detail);

			if (line_detail == MIN_DETAIL || !evaluated) {
				evaluated = true;
				evaluate(features, metabase, file_keys, layername, line_detail, compressed.size());
			}
		} else {
			mbtiles_write_tile(outdb, z, tx, ty, compressed.data(), compressed.size());
			return count;
		}
	}

	fprintf(stderr, "could not make tile %d/%u/%u small enough\n", z, tx, ty);
	exit(EXIT_FAILURE);
}

