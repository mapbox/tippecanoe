#include <iostream>
#include <fstream>
#include <string>
#include <stack>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <stdio.h>
#include <unistd.h>
#include <zlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
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

// https://github.com/mapbox/mapnik-vector-tile/blob/master/src/vector_tile_compression.hpp
static inline int compress(std::string const &input, std::string &output) {
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

	bool operator<(const coalesce &o) const {
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

struct pool_val *retrieve_string(char **f, struct pool *p, char *stringpool) {
	struct pool_val *ret;
	long long off;

	deserialize_long_long(f, &off);
	ret = pool(p, stringpool + off + 1, stringpool[off]);

	return ret;
}

void decode_meta(char **meta, char *stringpool, struct pool *keys, struct pool *values, struct pool *file_keys, std::vector<int> *intmeta, char *only) {
	int m;
	deserialize_int(meta, &m);

	int i;
	for (i = 0; i < m; i++) {
		struct pool_val *key = retrieve_string(meta, keys, stringpool);

		if (only != NULL && (strcmp(key->s, only) != 0)) {
			// XXX if evaluate ever works again, check whether this is sufficient
			(void) retrieve_string(meta, values, stringpool);
		} else {
			struct pool_val *value = retrieve_string(meta, values, stringpool);

			intmeta->push_back(key->n);
			intmeta->push_back(value->n);

			if (!is_pooled(file_keys, key->s, value->type)) {
				// Dup to retain after munmap
				pool(file_keys, strdup(key->s), value->type);
			}
		}
	}
}

mapnik::vector::tile create_tile(char **layernames, int line_detail, std::vector<std::vector<coalesce> > &features, long long *count, struct pool **keys, struct pool **values, int nlayers) {
	mapnik::vector::tile tile;

	int i;
	for (i = 0; i < nlayers; i++) {
		mapnik::vector::tile_layer *layer = tile.add_layers();

		layer->set_name(layernames[i]);
		layer->set_version(1);
		layer->set_extent(1 << line_detail);

		unsigned x;
		for (x = 0; x < features[i].size(); x++) {
			if (features[i][x].type == VT_LINE || features[i][x].type == VT_POLYGON) {
				features[i][x].geom = remove_noop(features[i][x].geom, features[i][x].type, 0);
			}

			mapnik::vector::tile_feature *feature = layer->add_features();

			if (features[i][x].type == VT_POINT) {
				feature->set_type(mapnik::vector::tile::Point);
			} else if (features[i][x].type == VT_LINE) {
				feature->set_type(mapnik::vector::tile::LineString);
			} else if (features[i][x].type == VT_POLYGON) {
				feature->set_type(mapnik::vector::tile::Polygon);
			} else {
				feature->set_type(mapnik::vector::tile::Unknown);
			}

			to_feature(features[i][x].geom, feature);
			*count += features[i][x].geom.size();

			unsigned y;
			for (y = 0; y < features[i][x].meta.size(); y++) {
				feature->add_tags(features[i][x].meta[y]);
			}
		}

		struct pool_val *pv;
		for (pv = keys[i]->head; pv != NULL; pv = pv->next) {
			layer->add_keys(pv->s, strlen(pv->s));
		}
		for (pv = values[i]->head; pv != NULL; pv = pv->next) {
			mapnik::vector::tile_value *tv = layer->add_values();

			if (pv->type == VT_NUMBER) {
				tv->set_double_value(atof(pv->s));
			} else if (pv->type == VT_BOOLEAN) {
				tv->set_bool_value(pv->s[0] == 't');
			} else {
				tv->set_string_value(pv->s);
			}
		}
	}

	return tile;
}

struct sll {
	char *name;
	long long val;

	bool operator<(const sll &o) const {
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

#if 0
void evaluate(std::vector<coalesce> &features, char *metabase, struct pool *file_keys, const char *layername, int line_detail, long long orig) {
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
		mapnik::vector::tile tile = create_tile(layername, line_detail, empty, &count, &keys, &values, 1); // XXX layer

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
	mapnik::vector::tile tile = create_tile(layername, line_detail, features, &count, &keys, &values, nlayers);

	std::string s;
	std::string compressed;

	tile.SerializeToString(&s);
	compress(s, compressed);
	fprintf(stderr, "geometry alone (-X) would be %lld\n", (long long) compressed.size());

	pool_free(&values);
	pool_free(&keys);
}
#endif

void rewrite(drawvec &geom, int z, int nextzoom, int file_maxzoom, long long *bbox, unsigned tx, unsigned ty, int buffer, int line_detail, int *within, long long *geompos, FILE **geomfile, const char *fname, signed char t, signed char layer, long long metastart, signed char feature_minzoom, int child_shards, int max_zoom_increment) {
	if (geom.size() > 0 && nextzoom <= file_maxzoom) {
		int xo, yo;
		int span = 1 << (nextzoom - z);

		// Get the feature bounding box in pixel (256) coordinates at the child zoom
		// in order to calculate which sub-tiles it can touch including the buffer.
		long long bbox2[4];
		int k;
		for (k = 0; k < 4; k++) {
			// Division instead of right-shift because coordinates can be negative
			bbox2[k] = bbox[k] / (1 << (32 - nextzoom - 8));
		}
		bbox2[0] -= buffer;
		bbox2[1] -= buffer;
		bbox2[2] += buffer;
		bbox2[3] += buffer;

		for (k = 0; k < 4; k++) {
			if (bbox2[k] < 0) {
				bbox2[k] = 0;
			}
			if (bbox2[k] >= 256 * span) {
				bbox2[k] = 256 * (span - 1);
			}

			bbox2[k] /= 256;
		}

		for (xo = bbox2[0]; xo <= bbox2[2]; xo++) {
			for (yo = bbox2[1]; yo <= bbox2[3]; yo++) {
				unsigned jx = tx * span + xo;
				unsigned jy = ty * span + yo;

				// j is the shard that the child tile's data is being written to.
				//
				// Be careful: We can't jump more zoom levels than max_zoom_increment
				// because that could break the constraint that each of the children
				// of the current tile must have its own shard, because the data for
				// the child tile must be contiguous within the shard.
				//
				// But it's OK to spread children across all the shards, not just
				// the four that would normally result from splitting one tile,
				// because it will go through all the shards when it does the
				// next zoom.
				//
				// If child_shards is a power of 2 but not a power of 4, this will
				// shard X more widely than Y. XXX Is there a better way to do this
				// without causing collisions?

			       int j = ((jx << max_zoom_increment) |
				        ((jy & ((1 << max_zoom_increment) - 1)))) & (child_shards - 1);

				{
					if (!within[j]) {
						serialize_int(geomfile[j], nextzoom, &geompos[j], fname);
						serialize_uint(geomfile[j], tx * span + xo, &geompos[j], fname);
						serialize_uint(geomfile[j], ty * span + yo, &geompos[j], fname);
						within[j] = 1;
					}

					// Offset from tile coordinates back to world coordinates
					unsigned sx = 0, sy = 0;
					if (z != 0) {
						sx = tx << (32 - z);
						sy = ty << (32 - z);
					}

					// printf("type %d, meta %lld\n", t, metastart);
					serialize_byte(geomfile[j], t, &geompos[j], fname);
					serialize_byte(geomfile[j], layer, &geompos[j], fname);
					serialize_long_long(geomfile[j], metastart, &geompos[j], fname);
					long long wx = initial_x, wy = initial_y;

					for (unsigned u = 0; u < geom.size(); u++) {
						serialize_byte(geomfile[j], geom[u].op, &geompos[j], fname);

						if (geom[u].op != VT_CLOSEPATH) {
							serialize_long_long(geomfile[j], ((geom[u].x + sx) >> geometry_scale) - (wx >> geometry_scale), &geompos[j], fname);
							serialize_long_long(geomfile[j], ((geom[u].y + sy) >> geometry_scale) - (wy >> geometry_scale), &geompos[j], fname);
							wx = geom[u].x + sx;
							wy = geom[u].y + sy;
						}
					}

					serialize_byte(geomfile[j], VT_END, &geompos[j], fname);
					serialize_byte(geomfile[j], feature_minzoom, &geompos[j], fname);
				}
			}
		}
	}
}

long long write_tile(char **geoms, char *metabase, char *stringpool, unsigned *file_bbox, int z, unsigned tx, unsigned ty, int detail, int min_detail, int basezoom, struct pool **file_keys, char **layernames, sqlite3 *outdb, double droprate, int buffer, const char *fname, FILE **geomfile, int file_minzoom, int file_maxzoom, double todo, char *geomstart, long long along, double gamma, int nlayers, char *prevent, int child_shards) {
	int line_detail;
	static bool evaluated = false;
	double oprogress = 0;
	double fraction = 1;

	char *og = *geoms;

	// XXX is there a way to do this without floating point?
	int max_zoom_increment = log(child_shards) / log(4);
	if (child_shards < 4 || max_zoom_increment < 1) {
		fprintf(stderr, "Internal error: %d shards, max zoom increment %d\n", child_shards, max_zoom_increment);
		exit(EXIT_FAILURE);
	}
	if ((((child_shards - 1) << 1) & child_shards) != child_shards) {
		fprintf(stderr, "Internal error: %d shards not a power of 2\n", child_shards);
		exit(EXIT_FAILURE);
	}

	int nextzoom = z + 1;
	if (nextzoom < file_minzoom) {
		if (z + max_zoom_increment > file_minzoom) {
			nextzoom = file_minzoom;
		} else {
			nextzoom = z + max_zoom_increment;
		}
	}

	for (line_detail = detail; line_detail >= min_detail || line_detail == detail; line_detail--) {
		GOOGLE_PROTOBUF_VERIFY_VERSION;

		struct pool keys1[nlayers], values1[nlayers];
		struct pool *keys[nlayers], *values[nlayers];
		int i;
		for (i = 0; i < nlayers; i++) {
			pool_init(&keys1[i], 0);
			pool_init(&values1[i], 0);

			keys[i] = &keys1[i];
			values[i] = &values1[i];
		}

		long long count = 0;
		// long long along = 0;
		double accum_area = 0;

		double interval = 0;
		double seq = 0;
		if (z < basezoom) {
			interval = exp(log(droprate) * (basezoom - z));
		}

		double fraction_accum = 0;

		unsigned long long previndex = 0;
		double scale = (double) (1LL << (64 - 2 * (z + 8)));
		double gap = 0;

		long long original_features = 0;
		long long unclipped_features = 0;

		std::vector<std::vector<coalesce> > features;
		for (i = 0; i < nlayers; i++) {
			features.push_back(std::vector<coalesce>());
		}

		int within[child_shards];
		long long geompos[child_shards];
		memset(within, '\0', sizeof(within));
		memset(geompos, '\0', sizeof(geompos));

		*geoms = og;

		while (1) {
			signed char t;
			deserialize_byte(geoms, &t);
			if (t < 0) {
				break;
			}

			signed char layer;
			deserialize_byte(geoms, &layer);

			long long metastart;
			deserialize_long_long(geoms, &metastart);
			char *meta = metabase + metastart;
			long long bbox[4];

			drawvec geom = decode_geometry(geoms, z, tx, ty, line_detail, bbox);

			signed char feature_minzoom;
			deserialize_byte(geoms, &feature_minzoom);

			double progress = floor((((*geoms - geomstart + along) / (double) todo) + z) / (file_maxzoom + 1) * 1000) / 10;
			if (progress != oprogress) {
				fprintf(stderr, "  %3.1f%%  %d/%u/%u  \r", progress, z, tx, ty);
				oprogress = progress;
			}

			original_features++;

			int quick = quick_check(bbox, z, line_detail, buffer);
			if (quick == 0) {
				continue;
			}

			if (quick != 1) {
				if (t == VT_LINE) {
					geom = clip_lines(geom, z, line_detail, buffer);
				}
				if (t == VT_POLYGON) {
					geom = clip_poly(geom, z, line_detail, buffer);
				}
				if (t == VT_POINT) {
					geom = clip_point(geom, z, line_detail, buffer);
				}

				geom = remove_noop(geom, t, 0);
			}

			if (geom.size() > 0) {
				unclipped_features++;
			}

			if (line_detail == detail && fraction == 1) { /* only write out the next zoom once, even if we retry */
				rewrite(geom, z, nextzoom, file_maxzoom, bbox, tx, ty, buffer, line_detail, within, geompos, geomfile, fname, t, layer, metastart, feature_minzoom, child_shards, max_zoom_increment);
			}

			if (z < file_minzoom) {
				continue;
			}

			if (t == VT_LINE && z + line_detail <= feature_minzoom) {
				continue;
			}

			if (t == VT_POINT && z < feature_minzoom && gamma < 0) {
				continue;
			}

			if (gamma >= 0 && (t == VT_POINT || (prevent['l' & 0xFF] && t == VT_LINE))) {
				seq++;
				if (seq >= 0) {
					seq -= interval;
				} else {
					continue;
				}

				if (gamma > 0) {
					unsigned long long index = encode(bbox[0] / 2 + bbox[2] / 2, bbox[1] / 2 + bbox[3] / 2);
					if (gap > 0) {
						if (index == previndex) {
							continue;  // Exact duplicate: can't fulfil the gap requirement
						}

						if (exp(log((index - previndex) / scale) * gamma) >= gap) {
							// Dot is further from the previous than the nth root of the gap,
							// so produce it, and choose a new gap at the next point.
							gap = 0;
						} else {
							continue;
						}
					} else {
						gap = (index - previndex) / scale;

						if (gap == 0) {
							continue;  // Exact duplicate: skip
						} else if (gap < 1) {
							continue;  // Narrow dot spacing: need to stretch out
						} else {
							gap = 0;  // Wider spacing than minimum: so pass through unchanged
						}
					}

					previndex = index;
				}
			}

			fraction_accum += fraction;
			if (fraction_accum < 1) {
				continue;
			}
			fraction_accum -= 1;

			bool reduced = false;
			if (t == VT_POLYGON) {
				geom = reduce_tiny_poly(geom, z, line_detail, &reduced, &accum_area);
			}

			if ((t == VT_LINE || t == VT_POLYGON) && !prevent['s' & 0xFF]) {
				if (!reduced) {
					if (t == VT_LINE) {
						geom = remove_noop(geom, t, 32 - z - line_detail);
					}

					geom = simplify_lines(geom, z, line_detail);
				}
			}

#if 0
			if (t == VT_LINE && z != basezoom) {
				geom = shrink_lines(geom, z, line_detail, basezoom, &along);
			}
#endif

			if (t == VT_LINE && !prevent['r' & 0xFF]) {
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
					c.index = 0;
					c.index2 = 0;
				}
				c.geom = geom;
				c.metasrc = meta;
				c.coalesced = false;

				decode_meta(&meta, stringpool, keys[layer], values[layer], file_keys[layer], &c.meta, NULL);
				features[layer].push_back(c);
			}
		}

		int j;
		for (j = 0; j < child_shards; j++) {
			if (within[j]) {
				serialize_byte(geomfile[j], -2, &geompos[j], fname);
				within[j] = 0;
			}
		}

		for (j = 0; j < nlayers; j++) {
			if (!prevent['o' & 0xFF]) {
				std::sort(features[j].begin(), features[j].end());
			}

			std::vector<coalesce> out;
			unsigned x;
			for (x = 0; x < features[j].size(); x++) {
				unsigned y = out.size() - 1;

#if 0
				if (out.size() > 0 && coalcmp(&features[j][x], &out[y]) < 0) {
					fprintf(stderr, "\nfeature out of order\n");
				}
#endif

				if (!prevent['c' & 0xFF] && out.size() > 0 && out[y].geom.size() + features[j][x].geom.size() < 20000 && coalcmp(&features[j][x], &out[y]) == 0 && features[j][x].type != VT_POINT) {
					unsigned z;
					for (z = 0; z < features[j][x].geom.size(); z++) {
						out[y].geom.push_back(features[j][x].geom[z]);
					}
					out[y].coalesced = true;
				} else {
					out.push_back(features[j][x]);
				}
			}
			features[j] = out;

			for (x = 0; x < features[j].size(); x++) {
				if (features[j][x].coalesced && features[j][x].type == VT_LINE) {
					features[j][x].geom = remove_noop(features[j][x].geom, features[j][x].type, 0);
					features[j][x].geom = simplify_lines(features[j][x].geom, 32, 0);
				}
			}
		}

		if (z == 0 && unclipped_features < original_features / 2) {
			fprintf(stderr, "\n\nMore than half the features were clipped away at zoom level 0.\n");
			fprintf(stderr, "Is your data in the wrong projection? It should be in WGS84/EPSG:4326.\n");
		}

		long long totalsize = 0;
		for (j = 0; j < nlayers; j++) {
			totalsize += features[j].size();
		}

		if (totalsize > 0) {
			if (totalsize > 200000 && !prevent['f' & 0xFF]) {
				fprintf(stderr, "tile %d/%u/%u has %lld features, >200000    \n", z, tx, ty, totalsize);
				fprintf(stderr, "Try using -z to set a higher base zoom level.\n");
				return -1;
			}

			mapnik::vector::tile tile = create_tile(layernames, line_detail, features, &count, keys, values, nlayers);

			int i;
			for (i = 0; i < nlayers; i++) {
				pool_free(&keys1[i]);
				pool_free(&values1[i]);
			}

			std::string s;
			std::string compressed;

			tile.SerializeToString(&s);
			compress(s, compressed);

			if (compressed.size() > 500000 && !prevent['k' & 0xFF]) {
				fprintf(stderr, "tile %d/%u/%u size is %lld with detail %d, >500000    \n", z, tx, ty, (long long) compressed.size(), line_detail);

				if (line_detail == min_detail || !evaluated) {
					evaluated = true;
#if 0
					evaluate(features[0], metabase, file_keys[0], layername, line_detail, compressed.size()); // XXX layer
#endif
				}

				if (prevent['d' & 0xFF]) {
					// The 95% is a guess to avoid too many retries
					// and probably actually varies based on how much duplicated metadata there is

					fraction = fraction * 500000 / compressed.size() * 0.95;
					fprintf(stderr, "Going to try keeping %0.2f%% of the features to make it fit\n", fraction * 100);
					line_detail++;  // to keep it the same when the loop decrements it
				}
			} else {
				mbtiles_write_tile(outdb, z, tx, ty, compressed.data(), compressed.size());
				return count;
			}
		} else {
			return count;
		}
	}

	fprintf(stderr, "could not make tile %d/%u/%u small enough\n", z, tx, ty);
	return -1;
}

int traverse_zooms(int *geomfd, off_t *geom_size, char *metabase, char *stringpool, unsigned *file_bbox, struct pool **file_keys, unsigned *midx, unsigned *midy, char **layernames, int maxzoom, int minzoom, sqlite3 *outdb, double droprate, int buffer, const char *fname, const char *tmpdir, double gamma, int nlayers, char *prevent, int full_detail, int low_detail, int min_detail) {
	int i;
	for (i = 0; i <= maxzoom; i++) {
		long long most = 0;

		FILE *sub[TEMP_FILES];
		int subfd[TEMP_FILES];
		int j;
		for (j = 0; j < TEMP_FILES; j++) {
			char geomname[strlen(tmpdir) + strlen("/geom2.XXXXXXXX") + 1];
			sprintf(geomname, "%s/geom%d.XXXXXXXX", tmpdir, j);
			subfd[j] = mkstemp(geomname);
			// printf("%s\n", geomname);
			if (subfd[j] < 0) {
				perror(geomname);
				exit(EXIT_FAILURE);
			}
			sub[j] = fopen(geomname, "wb");
			if (sub[j] == NULL) {
				perror(geomname);
				exit(EXIT_FAILURE);
			}
			unlink(geomname);
		}

		long long todo = 0;
		long long along = 0;
		for (j = 0; j < TEMP_FILES; j++) {
			todo += geom_size[j];
			printf("%d", geom_size[j] != 0);
		}
		printf("\n");

		// XXX Should be the number of temp files that have data,
		// capped by the number of processor threads we can actually run
		// or by the number of temp files divided by 4, or by
		// some number larger than 4 if we are trying to skip zooms.
		//
		// Will need to be a power of 2 to make sharding come out right.
		int threads = 1;

		int thread = 0;

		for (j = 0; j < TEMP_FILES; j++) {
			if (geomfd[j] < 0) {
				// only one source file for zoom level 0
				continue;
			}
			if (geom_size[j] == 0) {
				continue;
			}

			// printf("%lld of geom_size\n", (long long) geom_size[j]);

			char *geom = (char *) mmap(NULL, geom_size[j], PROT_READ, MAP_PRIVATE, geomfd[j], 0);
			if (geom == MAP_FAILED) {
				perror("mmap geom");
				exit(EXIT_FAILURE);
			}

			char *geomstart = geom;
			char *end = geom + geom_size[j];

			while (geom < end) {
				int z;
				unsigned x, y;

				deserialize_int(&geom, &z);
				deserialize_uint(&geom, &x);
				deserialize_uint(&geom, &y);

				// fprintf(stderr, "%d/%u/%u\n", z, x, y);

				long long len = write_tile(&geom, metabase, stringpool, file_bbox, z, x, y, z == maxzoom ? full_detail : low_detail, min_detail, maxzoom, file_keys, layernames, outdb, droprate, buffer, fname, sub + thread * (TEMP_FILES / threads), minzoom, maxzoom, todo, geomstart, along, gamma, nlayers, prevent, TEMP_FILES / threads);

				if (len < 0) {
					return z - 1;
				}

				if (z == maxzoom && len > most) {
					*midx = x;
					*midy = y;
					most = len;
				}
			}

			if (munmap(geomstart, geom_size[j]) != 0) {
				perror("munmap geom");
			}
			along += geom_size[j];
		}

		for (j = 0; j < TEMP_FILES; j++) {
			close(geomfd[j]);
			fclose(sub[j]);

			struct stat geomst;
			if (fstat(subfd[j], &geomst) != 0) {
				perror("stat geom\n");
				exit(EXIT_FAILURE);
			}

			geomfd[j] = subfd[j];
			geom_size[j] = geomst.st_size;
		}
	}

	fprintf(stderr, "\n");
	return maxzoom;
}
