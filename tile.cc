#include <iostream>
#include <fstream>
#include <string>
#include <stack>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <zlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <math.h>
#include <sqlite3.h>
#include <pthread.h>
#include <errno.h>
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

#define XSTRINGIFY(s) STRINGIFY(s)
#define STRINGIFY(s) #s

pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t var_lock = PTHREAD_MUTEX_INITIALIZER;

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
	long long original_seq;

	bool operator<(const coalesce &o) const {
		int cmp = coalindexcmp(this, &o);
		if (cmp < 0) {
			return true;
		} else {
			return false;
		}
	}
};

struct preservecmp {
	bool operator()(const struct coalesce &a, const struct coalesce &b) {
		return a.original_seq < b.original_seq;
	}
} preservecmp;

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

void decode_meta(char **meta, char *stringpool, struct pool *keys, struct pool *values, struct pool *file_keys, std::vector<int> *intmeta) {
	int m;
	deserialize_int(meta, &m);

	int i;
	for (i = 0; i < m; i++) {
		struct pool_val *key = retrieve_string(meta, keys, stringpool);
		struct pool_val *value = retrieve_string(meta, values, stringpool);

		intmeta->push_back(key->n);
		intmeta->push_back(value->n);

		if (!is_pooled(file_keys, key->s, value->type)) {
			if (pthread_mutex_lock(&var_lock) != 0) {
				perror("pthread_mutex_lock");
				exit(EXIT_FAILURE);
			}

			// Dup to retain after munmap
			pool(file_keys, strdup(key->s), value->type);

			if (pthread_mutex_unlock(&var_lock) != 0) {
				perror("pthread_mutex_unlock");
				exit(EXIT_FAILURE);
			}
		}
	}
}

static int is_integer(const char *s, long long *v) {
	errno = 0;
	char *endptr;

	*v = strtoll(s, &endptr, 0);
	if (*v == 0 && errno != 0) {
		return 0;
	}
	if ((*v == LLONG_MIN || *v == LLONG_MAX) && (errno == ERANGE)) {
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

mapnik::vector::tile create_tile(char **layernames, int line_detail, std::vector<std::vector<coalesce> > &features, long long *count, struct pool **keys, struct pool **values, int nlayers) {
	mapnik::vector::tile tile;

	int i;
	for (i = 0; i < nlayers; i++) {
		if (features[i].size() == 0) {
			continue;
		}

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
				long long v;
				if (is_integer(pv->s, &v)) {
					if (v >= 0) {
						tv->set_int_value(v);
					} else {
						tv->set_sint_value(v);
					}
				} else {
					tv->set_double_value(atof(pv->s));
				}
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

void rewrite(drawvec &geom, int z, int nextzoom, int maxzoom, long long *bbox, unsigned tx, unsigned ty, int buffer, int line_detail, int *within, long long *geompos, FILE **geomfile, const char *fname, signed char t, int layer, long long metastart, signed char feature_minzoom, int child_shards, int max_zoom_increment, long long seq, int tippecanoe_minzoom, int tippecanoe_maxzoom, int segment, unsigned *initial_x, unsigned *initial_y) {
	if (geom.size() > 0 && nextzoom <= maxzoom) {
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
					 ((jy & ((1 << max_zoom_increment) - 1)))) &
					(child_shards - 1);

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
					serialize_long_long(geomfile[j], seq, &geompos[j], fname);
					serialize_long_long(geomfile[j], (layer << 2) | ((tippecanoe_minzoom != -1) << 1) | (tippecanoe_maxzoom != -1), &geompos[j], fname);
					if (tippecanoe_minzoom != -1) {
						serialize_int(geomfile[j], tippecanoe_minzoom, geompos, fname);
					}
					if (tippecanoe_maxzoom != -1) {
						serialize_int(geomfile[j], tippecanoe_maxzoom, geompos, fname);
					}
					serialize_int(geomfile[j], segment, &geompos[j], fname);
					serialize_long_long(geomfile[j], metastart, &geompos[j], fname);
					long long wx = initial_x[segment], wy = initial_y[segment];

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

struct partial {
	drawvec geom;
	long long layer;
	char *meta;
	signed char t;
	int segment;
	long long original_seq;
	bool reduced;
	unsigned long long index;
	unsigned long long index2;
	int z;
	int line_detail;
	char *prevent;
	char *additional;
	int maxzoom;
};

struct partial_arg {
	std::vector<struct partial> *partials;
	int task;
	int tasks;
};

void *partial_feature_worker(void *v) {
	struct partial_arg *a = (struct partial_arg *) v;
	std::vector<struct partial> *partials = a->partials;

	for (unsigned i = a->task; i < (*partials).size(); i += a->tasks) {
		drawvec geom = (*partials)[i].geom;
		(*partials)[i].geom.clear();  // avoid keeping two copies in memory
		signed char t = (*partials)[i].t;
		int z = (*partials)[i].z;
		int line_detail = (*partials)[i].line_detail;
		char *prevent = (*partials)[i].prevent;
		char *additional = (*partials)[i].additional;
		int maxzoom = (*partials)[i].maxzoom;

		if ((t == VT_LINE || t == VT_POLYGON) && !(prevent[P_SIMPLIFY] || (z == maxzoom && prevent[P_SIMPLIFY_LOW]))) {
			if (1 /* !reduced */) {  // XXX why did this not simplify if reduced?
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

		if (t == VT_LINE && additional[A_REVERSE]) {
			geom = reorder_lines(geom);
		}

		to_tile_scale(geom, z, line_detail);

		if (t == VT_POLYGON) {
			// Scaling may have made the polygon degenerate.
			// Give Clipper a chance to try to fix it.
			geom = clean_or_clip_poly(geom, 0, 0, 0, false);
			geom = close_poly(geom);
		}

		// Worth skipping this if not coalescing anyway?
		if (geom.size() > 0) {
			(*partials)[i].index = encode(geom[0].x, geom[0].y);
			(*partials)[i].index2 = encode(geom[geom.size() - 1].x, geom[geom.size() - 1].y);

			// Anything numbered below the start of the line
			// can't possibly be the next feature.
			// We want lowest-but-not-under.
			if ((*partials)[i].index2 < (*partials)[i].index) {
				(*partials)[i].index2 = ~0LL;
			}
		} else {
			(*partials)[i].index = 0;
			(*partials)[i].index2 = 0;
		}

		(*partials)[i].geom = geom;
	}

	return NULL;
}

long long write_tile(char **geoms, char *metabase, char *stringpool, int z, unsigned tx, unsigned ty, int detail, int min_detail, int basezoom, struct pool **file_keys, char **layernames, sqlite3 *outdb, double droprate, int buffer, const char *fname, FILE **geomfile, int minzoom, int maxzoom, double todo, char *geomstart, volatile long long *along, double gamma, int nlayers, char *prevent, char *additional, int child_shards, long long *meta_off, long long *pool_off, unsigned *initial_x, unsigned *initial_y, volatile int *running) {
	int line_detail;
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
	if (nextzoom < minzoom) {
		if (z + max_zoom_increment > minzoom) {
			nextzoom = minzoom;
		} else {
			nextzoom = z + max_zoom_increment;
		}
	}

	static volatile double oprogress = 0;

	// This only loops if the tile data didn't fit, in which case the detail
	// goes down and the progress indicator goes backward for the next try.
	for (line_detail = detail; line_detail >= min_detail || line_detail == detail; line_detail--, oprogress = 0) {
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

		std::vector<struct partial> partials;
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

			long long original_seq;
			deserialize_long_long(geoms, &original_seq);

			long long layer;
			deserialize_long_long(geoms, &layer);
			int tippecanoe_minzoom = -1, tippecanoe_maxzoom = -1;
			if (layer & 2) {
				deserialize_int(geoms, &tippecanoe_minzoom);
			}
			if (layer & 1) {
				deserialize_int(geoms, &tippecanoe_maxzoom);
			}
			layer >>= 2;

			int segment;
			deserialize_int(geoms, &segment);

			long long metastart;
			deserialize_long_long(geoms, &metastart);
			char *meta = metabase + metastart + meta_off[segment];
			long long bbox[4];

			drawvec geom = decode_geometry(geoms, z, tx, ty, line_detail, bbox, initial_x[segment], initial_y[segment]);

			signed char feature_minzoom;
			deserialize_byte(geoms, &feature_minzoom);

			double progress = floor((((*geoms - geomstart + *along) / (double) todo) + z) / (maxzoom + 1) * 1000) / 10;
			if (progress >= oprogress + 0.1) {
				if (!quiet) {
					fprintf(stderr, "  %3.1f%%  %d/%u/%u  \r", progress, z, tx, ty);
				}
				oprogress = progress;
			}

			original_features++;

			if (z == 0 && t == VT_POLYGON) {
				geom = fix_polygon(geom);
			}

			int quick = quick_check(bbox, z, line_detail, buffer);
			if (quick == 0) {
				continue;
			}

			if (z == 0) {
				if (bbox[0] < 0 || bbox[2] > 1LL << 32) {
					// If the geometry extends off the edge of the world, concatenate on another copy
					// shifted by 360 degrees, and then make sure both copies get clipped down to size.

					unsigned n = geom.size();

					if (bbox[0] < 0) {
						for (unsigned i = 0; i < n; i++) {
							geom.push_back(draw(geom[i].op, geom[i].x + (1LL << 32), geom[i].y));
						}
					}

					if (bbox[2] > 1LL << 32) {
						for (unsigned i = 0; i < n; i++) {
							geom.push_back(draw(geom[i].op, geom[i].x - (1LL << 32), geom[i].y));
						}
					}

					bbox[0] = 0;
					bbox[2] = 1LL << 32;

					quick = -1;
				}
			}

			if (quick != 1) {
				if (t == VT_LINE) {
					geom = clip_lines(geom, z, line_detail, buffer);
				}
				if (t == VT_POLYGON) {
					geom = simple_clip_poly(geom, z, line_detail, buffer);
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
				rewrite(geom, z, nextzoom, maxzoom, bbox, tx, ty, buffer, line_detail, within, geompos, geomfile, fname, t, layer, metastart, feature_minzoom, child_shards, max_zoom_increment, original_seq, tippecanoe_minzoom, tippecanoe_maxzoom, segment, initial_x, initial_y);
			}

			if (z < minzoom) {
				continue;
			}

			if (tippecanoe_minzoom != -1 && z < tippecanoe_minzoom) {
				continue;
			}
			if (tippecanoe_maxzoom != -1 && z > tippecanoe_maxzoom) {
				continue;
			}

			if (t == VT_LINE && z + line_detail <= feature_minzoom) {
				continue;
			}

			if (t == VT_POINT && z < feature_minzoom && gamma < 0) {
				continue;
			}

			if (gamma >= 0 && (t == VT_POINT || (additional[A_LINE_DROP] && t == VT_LINE))) {
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

			if (geom.size() > 0) {
				partial p;
				p.geom = geom;
				p.layer = layer;
				p.meta = meta;
				p.t = t;
				p.segment = segment;
				p.original_seq = original_seq;
				p.reduced = reduced;
				p.z = z;
				p.line_detail = line_detail;
				p.prevent = prevent;
				p.additional = additional;
				p.maxzoom = maxzoom;
				partials.push_back(p);
			}
		}

		int tasks = ceil((double) CPUS / *running);
		if (tasks < 1) {
			tasks = 1;
		}

		pthread_t pthreads[tasks];
		partial_arg args[tasks];
		for (int i = 0; i < tasks; i++) {
			args[i].task = i;
			args[i].tasks = tasks;
			args[i].partials = &partials;

			if (tasks > 1) {
				if (pthread_create(&pthreads[i], NULL, partial_feature_worker, &args[i]) != 0) {
					perror("pthread_create");
					exit(EXIT_FAILURE);
				}
			} else {
				partial_feature_worker(&args[i]);
			}
		}

		if (tasks > 1) {
			for (int i = 0; i < tasks; i++) {
				void *retval;

				if (pthread_join(pthreads[i], &retval) != 0) {
					perror("pthread_join");
				}
			}
		}

		// This is serial because decode_meta() unifies duplicates
		for (unsigned i = 0; i < partials.size(); i++) {
			drawvec geom = partials[i].geom;
			partials[i].geom.clear();  // avoid keeping two copies in memory
			long long layer = partials[i].layer;
			char *meta = partials[i].meta;
			signed char t = partials[i].t;
			int segment = partials[i].segment;
			long long original_seq = partials[i].original_seq;

			if (t == VT_POINT || to_feature(geom, NULL)) {
				struct coalesce c;

				c.type = t;
				c.index = partials[i].index;
				c.index2 = partials[i].index2;
				c.geom = geom;
				c.metasrc = meta;
				c.coalesced = false;
				c.original_seq = original_seq;

				decode_meta(&meta, stringpool + pool_off[segment], keys[layer], values[layer], file_keys[layer], &c.meta);
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
			if (additional[A_REORDER]) {
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

				if (additional[A_COALESCE] && out.size() > 0 && out[y].geom.size() + features[j][x].geom.size() < 20000 && coalcmp(&features[j][x], &out[y]) == 0 && features[j][x].type != VT_POINT) {
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

			out.clear();
			for (x = 0; x < features[j].size(); x++) {
				if (features[j][x].coalesced && features[j][x].type == VT_LINE) {
					features[j][x].geom = remove_noop(features[j][x].geom, features[j][x].type, 0);
					features[j][x].geom = simplify_lines(features[j][x].geom, 32, 0);
				}

				if (features[j][x].geom.size() > 0) {
					out.push_back(features[j][x]);
				}
			}
			features[j] = out;

			if (prevent[P_INPUT_ORDER]) {
				std::sort(features[j].begin(), features[j].end(), preservecmp);
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
			if (totalsize > 200000 && !prevent[P_FEATURE_LIMIT]) {
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

			if (compressed.size() > 500000 && !prevent[P_KILOBYTE_LIMIT]) {
				if (!quiet) {
					fprintf(stderr, "tile %d/%u/%u size is %lld with detail %d, >500000    \n", z, tx, ty, (long long) compressed.size(), line_detail);
				}

				if (prevent[P_DYNAMIC_DROP]) {
					// The 95% is a guess to avoid too many retries
					// and probably actually varies based on how much duplicated metadata there is

					fraction = fraction * 500000 / compressed.size() * 0.95;
					if (!quiet) {
						fprintf(stderr, "Going to try keeping %0.2f%% of the features to make it fit\n", fraction * 100);
					}
					line_detail++;  // to keep it the same when the loop decrements it
				}
			} else {
				if (pthread_mutex_lock(&db_lock) != 0) {
					perror("pthread_mutex_lock");
					exit(EXIT_FAILURE);
				}

				mbtiles_write_tile(outdb, z, tx, ty, compressed.data(), compressed.size());

				if (pthread_mutex_unlock(&db_lock) != 0) {
					perror("pthread_mutex_unlock");
					exit(EXIT_FAILURE);
				}

				return count;
			}
		} else {
			int i;
			for (i = 0; i < nlayers; i++) {
				pool_free(&keys1[i]);
				pool_free(&values1[i]);
			}

			return count;
		}
	}

	fprintf(stderr, "could not make tile %d/%u/%u small enough\n", z, tx, ty);
	return -1;
}

struct task {
	int fileno;
	struct task *next;
};

struct write_tile_args {
	struct task *tasks;
	char *metabase;
	char *stringpool;
	int min_detail;
	int basezoom;
	struct pool **file_keys;
	char **layernames;
	sqlite3 *outdb;
	double droprate;
	int buffer;
	const char *fname;
	FILE **geomfile;
	double todo;
	volatile long long *along;
	double gamma;
	int nlayers;
	char *prevent;
	char *additional;
	int child_shards;
	int *geomfd;
	off_t *geom_size;
	volatile unsigned *midx;
	volatile unsigned *midy;
	int maxzoom;
	int minzoom;
	int full_detail;
	int low_detail;
	volatile long long *most;
	long long *meta_off;
	long long *pool_off;
	unsigned *initial_x;
	unsigned *initial_y;
	volatile int *running;
};

void *run_thread(void *vargs) {
	write_tile_args *arg = (write_tile_args *) vargs;
	struct task *task;

	for (task = arg->tasks; task != NULL; task = task->next) {
		int j = task->fileno;

		if (arg->geomfd[j] < 0) {
			// only one source file for zoom level 0
			continue;
		}
		if (arg->geom_size[j] == 0) {
			continue;
		}

		// printf("%lld of geom_size\n", (long long) geom_size[j]);

		char *geom = (char *) mmap(NULL, arg->geom_size[j], PROT_READ, MAP_PRIVATE, arg->geomfd[j], 0);
		if (geom == MAP_FAILED) {
			perror("mmap geom");
			exit(EXIT_FAILURE);
		}

		char *geomstart = geom;
		char *end = geom + arg->geom_size[j];
		char *prevgeom = geom;

		while (geom < end) {
			int z;
			unsigned x, y;

			deserialize_int(&geom, &z);
			deserialize_uint(&geom, &x);
			deserialize_uint(&geom, &y);

			// fprintf(stderr, "%d/%u/%u\n", z, x, y);

			long long len = write_tile(&geom, arg->metabase, arg->stringpool, z, x, y, z == arg->maxzoom ? arg->full_detail : arg->low_detail, arg->min_detail, arg->basezoom, arg->file_keys, arg->layernames, arg->outdb, arg->droprate, arg->buffer, arg->fname, arg->geomfile, arg->minzoom, arg->maxzoom, arg->todo, geomstart, arg->along, arg->gamma, arg->nlayers, arg->prevent, arg->additional, arg->child_shards, arg->meta_off, arg->pool_off, arg->initial_x, arg->initial_y, arg->running);

			if (len < 0) {
				int *err = (int *) malloc(sizeof(int));
				*err = z - 1;
				return err;
			}

			if (pthread_mutex_lock(&var_lock) != 0) {
				perror("pthread_mutex_lock");
				exit(EXIT_FAILURE);
			}

			if (z == arg->maxzoom && len > *arg->most) {
				*arg->midx = x;
				*arg->midy = y;
				*arg->most = len;
			}

			*arg->along += geom - prevgeom;
			prevgeom = geom;

			if (pthread_mutex_unlock(&var_lock) != 0) {
				perror("pthread_mutex_unlock");
				exit(EXIT_FAILURE);
			}
		}

		if (munmap(geomstart, arg->geom_size[j]) != 0) {
			perror("munmap geom");
		}
	}

	arg->running--;
	return NULL;
}

int traverse_zooms(int *geomfd, off_t *geom_size, char *metabase, char *stringpool, struct pool **file_keys, unsigned *midx, unsigned *midy, char **layernames, int maxzoom, int minzoom, int basezoom, sqlite3 *outdb, double droprate, int buffer, const char *fname, const char *tmpdir, double gamma, int nlayers, char *prevent, char *additional, int full_detail, int low_detail, int min_detail, long long *meta_off, long long *pool_off, unsigned *initial_x, unsigned *initial_y) {
	int i;
	for (i = 0; i <= maxzoom; i++) {
		long long most = 0;

		FILE *sub[TEMP_FILES];
		int subfd[TEMP_FILES];
		int j;
		for (j = 0; j < TEMP_FILES; j++) {
			char geomname[strlen(tmpdir) + strlen("/geom.XXXXXXXX" XSTRINGIFY(INT_MAX)) + 1];
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

		int useful_threads = 0;
		long long todo = 0;
		long long along = 0;
		for (j = 0; j < TEMP_FILES; j++) {
			todo += geom_size[j];
			if (geom_size[j] > 0) {
				useful_threads++;
			}
		}

		int threads = CPUS;
		if (threads > TEMP_FILES / 4) {
			threads = TEMP_FILES / 4;
		}
		// XXX is it useful to divide further if we know we are skipping
		// some zoom levels? Is it faster to have fewer CPUs working on
		// sharding, but more deeply, or fewer CPUs, less deeply?
		if (threads > useful_threads) {
			threads = useful_threads;
		}
		// Round down to a power of 2
		threads = 1 << (int) (log(threads) / log(2));

		// Assign temporary files to threads

		struct task tasks[TEMP_FILES];
		struct dispatch {
			struct task *tasks;
			long long todo;
			struct dispatch *next;
		} dispatches[threads];
		struct dispatch *dispatch_head = &dispatches[0];
		for (j = 0; j < threads; j++) {
			dispatches[j].tasks = NULL;
			dispatches[j].todo = 0;
			if (j + 1 < threads) {
				dispatches[j].next = &dispatches[j + 1];
			} else {
				dispatches[j].next = NULL;
			}
		}

		for (j = 0; j < TEMP_FILES; j++) {
			if (geom_size[j] == 0) {
				continue;
			}

			tasks[j].fileno = j;
			tasks[j].next = dispatch_head->tasks;
			dispatch_head->tasks = &tasks[j];
			dispatch_head->todo += geom_size[j];

			struct dispatch *here = dispatch_head;
			dispatch_head = dispatch_head->next;

			dispatch **d;
			for (d = &dispatch_head; *d != NULL; d = &((*d)->next)) {
				if (here->todo < (*d)->todo) {
					break;
				}
			}

			here->next = *d;
			*d = here;
		}

		pthread_t pthreads[threads];
		write_tile_args args[threads];
		int running = threads;

		int thread;
		for (thread = 0; thread < threads; thread++) {
			args[thread].metabase = metabase;
			args[thread].stringpool = stringpool;
			args[thread].min_detail = min_detail;
			args[thread].basezoom = basezoom;
			args[thread].file_keys = file_keys;  // locked with var_lock
			args[thread].layernames = layernames;
			args[thread].outdb = outdb;  // locked with db_lock
			args[thread].droprate = droprate;
			args[thread].buffer = buffer;
			args[thread].fname = fname;
			args[thread].geomfile = sub + thread * (TEMP_FILES / threads);
			args[thread].todo = todo;
			args[thread].along = &along;  // locked with var_lock
			args[thread].gamma = gamma;
			args[thread].nlayers = nlayers;
			args[thread].prevent = prevent;
			args[thread].additional = additional;
			args[thread].child_shards = TEMP_FILES / threads;

			args[thread].geomfd = geomfd;
			args[thread].geom_size = geom_size;
			args[thread].midx = midx;  // locked with var_lock
			args[thread].midy = midy;  // locked with var_lock
			args[thread].maxzoom = maxzoom;
			args[thread].minzoom = minzoom;
			args[thread].full_detail = full_detail;
			args[thread].low_detail = low_detail;
			args[thread].most = &most;  // locked with var_lock
			args[thread].meta_off = meta_off;
			args[thread].pool_off = pool_off;
			args[thread].initial_x = initial_x;
			args[thread].initial_y = initial_y;

			args[thread].tasks = dispatches[thread].tasks;
			args[thread].running = &running;

			if (pthread_create(&pthreads[thread], NULL, run_thread, &args[thread]) != 0) {
				perror("pthread_create");
				exit(EXIT_FAILURE);
			}
		}

		int err = INT_MAX;

		for (thread = 0; thread < threads; thread++) {
			void *retval;

			if (pthread_join(pthreads[thread], &retval) != 0) {
				perror("pthread_join");
			}

			if (retval != NULL) {
				err = *((int *) retval);
			}
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

		if (err != INT_MAX) {
			return err;
		}
	}

	if (!quiet) {
		fprintf(stderr, "\n");
	}
	return maxzoom;
}
