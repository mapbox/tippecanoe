#ifdef __APPLE__
#define _DARWIN_UNLIMITED_STREAMS
#endif

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
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <zlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <cmath>
#include <sqlite3.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "mvt.hpp"
#include "mbtiles.hpp"
#include "dirtiles.hpp"
#include "geometry.hpp"
#include "tile.hpp"
#include "pool.hpp"
#include "projection.hpp"
#include "serial.hpp"
#include "options.hpp"
#include "main.hpp"
#include "write_json.hpp"
#include "milo/dtoa_milo.h"
#include "evaluator.hpp"

extern "C" {
#include "jsonpull/jsonpull.h"
}

#include "plugin.hpp"

#define CMD_BITS 3

// Offset coordinates to keep them positive
#define COORD_OFFSET (4LL << 32)
#define SHIFT_RIGHT(a) ((((a) + COORD_OFFSET) >> geometry_scale) - (COORD_OFFSET >> geometry_scale))

#define XSTRINGIFY(s) STRINGIFY(s)
#define STRINGIFY(s) #s

pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t var_lock = PTHREAD_MUTEX_INITIALIZER;

std::vector<mvt_geometry> to_feature(drawvec &geom) {
	std::vector<mvt_geometry> out;

	for (size_t i = 0; i < geom.size(); i++) {
		out.push_back(mvt_geometry(geom[i].op, geom[i].x, geom[i].y));
	}

	return out;
}

bool draws_something(drawvec &geom) {
	for (size_t i = 1; i < geom.size(); i++) {
		if (geom[i].op == VT_LINETO && (geom[i].x != geom[i - 1].x || geom[i].y != geom[i - 1].y)) {
			return true;
		}
	}

	return false;
}

static int metacmp(const std::vector<long long> &keys1, const std::vector<long long> &values1, char *stringpool1, const std::vector<long long> &keys2, const std::vector<long long> &values2, char *stringpool2);
int coalindexcmp(const struct coalesce *c1, const struct coalesce *c2);

struct coalesce {
	char *stringpool = NULL;
	std::vector<long long> keys = std::vector<long long>();
	std::vector<long long> values = std::vector<long long>();
	std::vector<std::string> full_keys = std::vector<std::string>();
	std::vector<serial_val> full_values = std::vector<serial_val>();
	drawvec geom = drawvec();
	unsigned long long index = 0;
	long long original_seq = 0;
	int type = 0;
	bool coalesced = false;
	double spacing = 0;
	bool has_id = false;
	unsigned long long id = 0;

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

	if (c1->has_id != c2->has_id) {
		return (int) c1->has_id - (int) c2->has_id;
	}

	if (c1->has_id && c2->has_id) {
		if (c1->id < c2->id) {
			return -1;
		}
		if (c1->id > c2->id) {
			return 1;
		}
	}

	cmp = metacmp(c1->keys, c1->values, c1->stringpool, c2->keys, c2->values, c2->stringpool);
	if (cmp != 0) {
		return cmp;
	}

	if (c1->full_keys.size() < c2->full_keys.size()) {
		return -1;
	} else if (c1->full_keys.size() > c2->full_keys.size()) {
		return 1;
	}

	for (size_t i = 0; i < c1->full_keys.size(); i++) {
		if (c1->full_keys[i] < c2->full_keys[i]) {
			return -1;
		} else if (c1->full_keys[i] > c2->full_keys[i]) {
			return 1;
		}

		if (c1->full_values[i].type < c2->full_values[i].type) {
			return -1;
		} else if (c1->full_values[i].type > c2->full_values[i].type) {
			return 1;
		}

		if (c1->full_values[i].s < c2->full_values[i].s) {
			return -1;
		} else if (c1->full_values[i].s > c2->full_values[i].s) {
			return 1;
		}
	}

	return 0;
}

int coalindexcmp(const struct coalesce *c1, const struct coalesce *c2) {
	int cmp = coalcmp((const void *) c1, (const void *) c2);

	if (cmp == 0) {
		if (c1->index < c2->index) {
			return -1;
		} else if (c1->index > c2->index) {
			return 1;
		}

		if (c1->geom < c2->geom) {
			return -1;
		} else if (c1->geom > c2->geom) {
			return 1;
		}
	}

	return cmp;
}

mvt_value retrieve_string(long long off, char *stringpool, int *otype) {
	int type = stringpool[off];
	char *s = stringpool + off + 1;

	if (otype != NULL) {
		*otype = type;
	}

	return stringified_to_mvt_value(type, s);
}

void decode_meta(std::vector<long long> const &metakeys, std::vector<long long> const &metavals, char *stringpool, mvt_layer &layer, mvt_feature &feature) {
	size_t i;
	for (i = 0; i < metakeys.size(); i++) {
		int otype;
		mvt_value key = retrieve_string(metakeys[i], stringpool, NULL);
		mvt_value value = retrieve_string(metavals[i], stringpool, &otype);

		layer.tag(feature, key.string_value, value);
	}
}

static int metacmp(const std::vector<long long> &keys1, const std::vector<long long> &values1, char *stringpool1, const std::vector<long long> &keys2, const std::vector<long long> &values2, char *stringpool2) {
	size_t i;
	for (i = 0; i < keys1.size() && i < keys2.size(); i++) {
		mvt_value key1 = retrieve_string(keys1[i], stringpool1, NULL);
		mvt_value key2 = retrieve_string(keys2[i], stringpool2, NULL);

		if (key1.string_value < key2.string_value) {
			return -1;
		} else if (key1.string_value > key2.string_value) {
			return 1;
		}

		long long off1 = values1[i];
		int type1 = stringpool1[off1];
		char *s1 = stringpool1 + off1 + 1;

		long long off2 = values2[i];
		int type2 = stringpool2[off2];
		char *s2 = stringpool2 + off2 + 1;

		if (type1 != type2) {
			return type1 - type2;
		}
		int cmp = strcmp(s1, s2);
		if (cmp != 0) {
			return cmp;
		}
	}

	if (keys1.size() < keys2.size()) {
		return -1;
	} else if (keys1.size() > keys2.size()) {
		return 1;
	} else {
		return 0;
	}
}

void rewrite(drawvec &geom, int z, int nextzoom, int maxzoom, long long *bbox, unsigned tx, unsigned ty, int buffer, int *within, std::atomic<long long> *geompos, FILE **geomfile, const char *fname, signed char t, int layer, long long metastart, signed char feature_minzoom, int child_shards, int max_zoom_increment, long long seq, int tippecanoe_minzoom, int tippecanoe_maxzoom, int segment, unsigned *initial_x, unsigned *initial_y, std::vector<long long> &metakeys, std::vector<long long> &metavals, bool has_id, unsigned long long id, unsigned long long index, long long extent) {
	if (geom.size() > 0 && (nextzoom <= maxzoom || additional[A_EXTEND_ZOOMS])) {
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
		// Decrement the top and left edges so that any features that are
		// touching the edge can potentially be included in the adjacent tiles too.
		bbox2[0] -= buffer + 1;
		bbox2[1] -= buffer + 1;
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

		// Offset from tile coordinates back to world coordinates
		unsigned sx = 0, sy = 0;
		if (z != 0) {
			sx = tx << (32 - z);
			sy = ty << (32 - z);
		}

		drawvec geom2;
		for (size_t i = 0; i < geom.size(); i++) {
			geom2.push_back(draw(geom[i].op, SHIFT_RIGHT(geom[i].x + sx), SHIFT_RIGHT(geom[i].y + sy)));
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

					serial_feature sf;
					sf.layer = layer;
					sf.segment = segment;
					sf.seq = seq;
					sf.t = t;
					sf.has_id = has_id;
					sf.id = id;
					sf.has_tippecanoe_minzoom = tippecanoe_minzoom != -1;
					sf.tippecanoe_minzoom = tippecanoe_minzoom;
					sf.has_tippecanoe_maxzoom = tippecanoe_maxzoom != -1;
					sf.tippecanoe_maxzoom = tippecanoe_maxzoom;
					sf.metapos = metastart;
					sf.geometry = geom2;
					sf.index = index;
					sf.extent = extent;
					sf.feature_minzoom = feature_minzoom;

					if (metastart < 0) {
						for (size_t i = 0; i < metakeys.size(); i++) {
							sf.keys.push_back(metakeys[i]);
							sf.values.push_back(metavals[i]);
						}
					}

					serialize_feature(geomfile[j], &sf, &geompos[j], fname, SHIFT_RIGHT(initial_x[segment]), SHIFT_RIGHT(initial_y[segment]), true);
				}
			}
		}
	}
}

struct accum_state {
	double sum = 0;
	double count = 0;
};

struct partial {
	std::vector<drawvec> geoms = std::vector<drawvec>();
	std::vector<long long> keys = std::vector<long long>();
	std::vector<long long> values = std::vector<long long>();
	std::vector<std::string> full_keys = std::vector<std::string>();
	std::vector<serial_val> full_values = std::vector<serial_val>();
	std::vector<ssize_t> arc_polygon = std::vector<ssize_t>();
	long long layer = 0;
	long long original_seq = 0;
	unsigned long long index = 0;
	int segment = 0;
	bool reduced = 0;
	int z = 0;
	int line_detail = 0;
	int maxzoom = 0;
	double spacing = 0;
	double simplification = 0;
	signed char t = 0;
	unsigned long long id = 0;
	bool has_id = 0;
	ssize_t renamed = 0;
	long long extent = 0;
	long long clustered = 0;
	std::set<std::string> need_tilestats;
	std::map<std::string, accum_state> attribute_accum_state;
};

struct partial_arg {
	std::vector<struct partial> *partials = NULL;
	int task = 0;
	int tasks = 0;
	drawvec *shared_nodes;
};

drawvec revive_polygon(drawvec &geom, double area, int z, int detail) {
	// From area in world coordinates to area in tile coordinates
	long long divisor = 1LL << (32 - detail - z);
	area /= divisor * divisor;

	if (area == 0) {
		return drawvec();
	}

	int height = ceil(sqrt(area));
	int width = round(area / height);
	if (width == 0) {
		width = 1;
	}

	long long sx = 0, sy = 0, n = 0;
	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO || geom[i].op == VT_LINETO) {
			sx += geom[i].x;
			sy += geom[i].y;
			n++;
		}
	}

	if (n > 0) {
		sx /= n;
		sy /= n;

		drawvec out;
		out.push_back(draw(VT_MOVETO, sx - (width / 2), sy - (height / 2)));
		out.push_back(draw(VT_LINETO, sx - (width / 2) + width, sy - (height / 2)));
		out.push_back(draw(VT_LINETO, sx - (width / 2) + width, sy - (height / 2) + height));
		out.push_back(draw(VT_LINETO, sx - (width / 2), sy - (height / 2) + height));
		out.push_back(draw(VT_LINETO, sx - (width / 2), sy - (height / 2)));

		return out;
	} else {
		return drawvec();
	}
}

void *partial_feature_worker(void *v) {
	struct partial_arg *a = (struct partial_arg *) v;
	std::vector<struct partial> *partials = a->partials;

	for (size_t i = a->task; i < (*partials).size(); i += a->tasks) {
		drawvec geom;

		for (size_t j = 0; j < (*partials)[i].geoms.size(); j++) {
			for (size_t k = 0; k < (*partials)[i].geoms[j].size(); k++) {
				geom.push_back((*partials)[i].geoms[j][k]);
			}
		}

		(*partials)[i].geoms.clear();  // avoid keeping two copies in memory
		signed char t = (*partials)[i].t;
		int z = (*partials)[i].z;
		int line_detail = (*partials)[i].line_detail;
		int maxzoom = (*partials)[i].maxzoom;

		if (additional[A_GRID_LOW_ZOOMS] && z < maxzoom) {
			geom = stairstep(geom, z, line_detail);
		}

		double area = 0;
		if (t == VT_POLYGON) {
			area = get_mp_area(geom);
		}

		if ((t == VT_LINE || t == VT_POLYGON) && !(prevent[P_SIMPLIFY] || (z == maxzoom && prevent[P_SIMPLIFY_LOW]) || (z < maxzoom && additional[A_GRID_LOW_ZOOMS]))) {
			if (1 /* !reduced */) {  // XXX why did this not simplify if reduced?
				if (t == VT_LINE) {
					geom = remove_noop(geom, t, 32 - z - line_detail);
				}

				bool already_marked = false;
				if (additional[A_DETECT_SHARED_BORDERS] && t == VT_POLYGON) {
					already_marked = true;
				}

				if (!already_marked) {
					drawvec ngeom = simplify_lines(geom, z, line_detail, !(prevent[P_CLIPPING] || prevent[P_DUPLICATION]), (*partials)[i].simplification, t == VT_POLYGON ? 4 : 0, *(a->shared_nodes));

					if (t != VT_POLYGON || ngeom.size() >= 3) {
						geom = ngeom;
					}
				}
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

		std::vector<drawvec> geoms;
		geoms.push_back(geom);

		if (t == VT_POLYGON) {
			// Scaling may have made the polygon degenerate.
			// Give Clipper a chance to try to fix it.
			for (size_t g = 0; g < geoms.size(); g++) {
				drawvec before = geoms[g];
				geoms[g] = clean_or_clip_poly(geoms[g], 0, 0, false);
				if (additional[A_DEBUG_POLYGON]) {
					check_polygon(geoms[g]);
				}

				if (geoms[g].size() < 3) {
					if (area > 0) {
						geoms[g] = revive_polygon(before, area / geoms.size(), z, line_detail);
					} else {
						geoms[g].clear();
					}
				}
			}
		}

		(*partials)[i].index = i;
		(*partials)[i].geoms = geoms;
	}

	return NULL;
}

int manage_gap(unsigned long long index, unsigned long long *previndex, double scale, double gamma, double *gap) {
	if (gamma > 0) {
		if (*gap > 0) {
			if (index == *previndex) {
				return 1;  // Exact duplicate: can't fulfil the gap requirement
			}

			if (index < *previndex || std::exp(std::log((index - *previndex) / scale) * gamma) >= *gap) {
				// Dot is further from the previous than the nth root of the gap,
				// so produce it, and choose a new gap at the next point.
				*gap = 0;
			} else {
				return 1;
			}
		} else if (index >= *previndex) {
			*gap = (index - *previndex) / scale;

			if (*gap == 0) {
				return 1;  // Exact duplicate: skip
			} else if (*gap < 1) {
				return 1;  // Narrow dot spacing: need to stretch out
			} else {
				*gap = 0;  // Wider spacing than minimum: so pass through unchanged
			}
		}

		*previndex = index;
	}

	return 0;
}

// Does not fix up moveto/lineto
static drawvec reverse_subring(drawvec const &dv) {
	drawvec out;

	for (size_t i = dv.size(); i > 0; i--) {
		out.push_back(dv[i - 1]);
	}

	return out;
}

struct edge {
	unsigned x1 = 0;
	unsigned y1 = 0;
	unsigned x2 = 0;
	unsigned y2 = 0;
	unsigned ring = 0;

	edge(unsigned _x1, unsigned _y1, unsigned _x2, unsigned _y2, unsigned _ring) {
		x1 = _x1;
		y1 = _y1;
		x2 = _x2;
		y2 = _y2;
		ring = _ring;
	}

	bool operator<(const edge &s) const {
		long long cmp = (long long) y1 - s.y1;
		if (cmp == 0) {
			cmp = (long long) x1 - s.x1;
		}
		if (cmp == 0) {
			cmp = (long long) y2 - s.y2;
		}
		if (cmp == 0) {
			cmp = (long long) x2 - s.x2;
		}
		return cmp < 0;
	}
};

struct edgecmp_ring {
	bool operator()(const edge &a, const edge &b) {
		long long cmp = (long long) a.y1 - b.y1;
		if (cmp == 0) {
			cmp = (long long) a.x1 - b.x1;
		}
		if (cmp == 0) {
			cmp = (long long) a.y2 - b.y2;
		}
		if (cmp == 0) {
			cmp = (long long) a.x2 - b.x2;
		}
		if (cmp == 0) {
			cmp = (long long) a.ring - b.ring;
		}
		return cmp < 0;
	}
} edgecmp_ring;

bool edges_same(std::pair<std::vector<edge>::iterator, std::vector<edge>::iterator> e1, std::pair<std::vector<edge>::iterator, std::vector<edge>::iterator> e2) {
	if ((e2.second - e2.first) != (e1.second - e1.first)) {
		return false;
	}

	while (e1.first != e1.second) {
		if (e1.first->ring != e2.first->ring) {
			return false;
		}

		++e1.first;
		++e2.first;
	}

	return true;
}

bool find_common_edges(std::vector<partial> &partials, int z, int line_detail, double simplification, int maxzoom, double merge_fraction) {
	size_t merge_count = ceil((1 - merge_fraction) * partials.size());

	for (size_t i = 0; i < partials.size(); i++) {
		if (partials[i].t == VT_POLYGON) {
			for (size_t j = 0; j < partials[i].geoms.size(); j++) {
				drawvec &g = partials[i].geoms[j];
				drawvec out;

				for (size_t k = 0; k < g.size(); k++) {
					if (g[k].op == VT_LINETO && k > 0 && g[k - 1] == g[k]) {
						;
					} else {
						out.push_back(g[k]);
					}
				}

				partials[i].geoms[j] = out;
			}
		}
	}

	// Construct a mapping from all polygon edges to the set of rings
	// that each edge appears in. (The ring number is across all polygons;
	// we don't need to look it back up, just to tell where it changes.)

	std::vector<edge> edges;
	size_t ring = 0;
	for (size_t i = 0; i < partials.size(); i++) {
		if (partials[i].t == VT_POLYGON) {
			for (size_t j = 0; j < partials[i].geoms.size(); j++) {
				for (size_t k = 0; k + 1 < partials[i].geoms[j].size(); k++) {
					if (partials[i].geoms[j][k].op == VT_MOVETO) {
						ring++;
					}

					if (partials[i].geoms[j][k + 1].op == VT_LINETO) {
						drawvec dv;
						if (partials[i].geoms[j][k] < partials[i].geoms[j][k + 1]) {
							dv.push_back(partials[i].geoms[j][k]);
							dv.push_back(partials[i].geoms[j][k + 1]);
						} else {
							dv.push_back(partials[i].geoms[j][k + 1]);
							dv.push_back(partials[i].geoms[j][k]);
						}

						edges.push_back(edge(dv[0].x, dv[0].y, dv[1].x, dv[1].y, ring));
					}
				}
			}
		}
	}

	std::sort(edges.begin(), edges.end(), edgecmp_ring);
	std::set<draw> necessaries;

	// Now mark all the points where the set of rings using the edge on one side
	// is not the same as the set of rings using the edge on the other side.

	for (size_t i = 0; i < partials.size(); i++) {
		if (partials[i].t == VT_POLYGON) {
			for (size_t j = 0; j < partials[i].geoms.size(); j++) {
				drawvec &g = partials[i].geoms[j];

				for (size_t k = 0; k < g.size(); k++) {
					g[k].necessary = 0;
				}

				for (size_t a = 0; a < g.size(); a++) {
					if (g[a].op == VT_MOVETO) {
						size_t b;

						for (b = a + 1; b < g.size(); b++) {
							if (g[b].op != VT_LINETO) {
								break;
							}
						}

						// -1 because of duplication at the end
						size_t s = b - a - 1;

						if (s > 0) {
							drawvec left;
							if (g[a + (s - 1) % s] < g[a]) {
								left.push_back(g[a + (s - 1) % s]);
								left.push_back(g[a]);
							} else {
								left.push_back(g[a]);
								left.push_back(g[a + (s - 1) % s]);
							}
							if (left[1] < left[0]) {
								fprintf(stderr, "left misordered\n");
							}
							std::pair<std::vector<edge>::iterator, std::vector<edge>::iterator> e1 = std::equal_range(edges.begin(), edges.end(), edge(left[0].x, left[0].y, left[1].x, left[1].y, 0));

							for (size_t k = 0; k < s; k++) {
								drawvec right;

								if (g[a + k] < g[a + k + 1]) {
									right.push_back(g[a + k]);
									right.push_back(g[a + k + 1]);
								} else {
									right.push_back(g[a + k + 1]);
									right.push_back(g[a + k]);
								}

								std::pair<std::vector<edge>::iterator, std::vector<edge>::iterator> e2 = std::equal_range(edges.begin(), edges.end(), edge(right[0].x, right[0].y, right[1].x, right[1].y, 0));

								if (right[1] < right[0]) {
									fprintf(stderr, "left misordered\n");
								}

								if (e1.first == e1.second || e2.first == e2.second) {
									fprintf(stderr, "Internal error: polygon edge lookup failed for %lld,%lld to %lld,%lld or %lld,%lld to %lld,%lld\n", left[0].x, left[0].y, left[1].x, left[1].y, right[0].x, right[0].y, right[1].x, right[1].y);
									exit(EXIT_FAILURE);
								}

								if (!edges_same(e1, e2)) {
									g[a + k].necessary = 1;
									necessaries.insert(g[a + k]);
								}

								e1 = e2;
							}
						}

						a = b - 1;
					}
				}
			}
		}
	}

	edges.clear();
	std::map<drawvec, size_t> arcs;
	std::multimap<ssize_t, size_t> merge_candidates;  // from arc to partial

	// Roll rings that include a necessary point around so they start at one

	for (size_t i = 0; i < partials.size(); i++) {
		if (partials[i].t == VT_POLYGON) {
			for (size_t j = 0; j < partials[i].geoms.size(); j++) {
				drawvec &g = partials[i].geoms[j];

				for (size_t k = 0; k < g.size(); k++) {
					if (necessaries.count(g[k]) != 0) {
						g[k].necessary = 1;
					}
				}

				for (size_t k = 0; k < g.size(); k++) {
					if (g[k].op == VT_MOVETO) {
						ssize_t necessary = -1;
						ssize_t lowest = k;
						size_t l;
						for (l = k + 1; l < g.size(); l++) {
							if (g[l].op != VT_LINETO) {
								break;
							}

							if (g[l].necessary) {
								necessary = l;
							}
							if (g[l] < g[lowest]) {
								lowest = l;
							}
						}

						if (necessary < 0) {
							necessary = lowest;
							// Add a necessary marker if there was none in the ring,
							// so the arc code below can find it.
							g[lowest].necessary = 1;
						}

						{
							drawvec tmp;

							// l - 1 because the endpoint is duplicated
							for (size_t m = necessary; m < l - 1; m++) {
								tmp.push_back(g[m]);
							}
							for (ssize_t m = k; m < necessary; m++) {
								tmp.push_back(g[m]);
							}

							// replace the endpoint
							tmp.push_back(g[necessary]);

							if (tmp.size() != l - k) {
								fprintf(stderr, "internal error shifting ring\n");
								exit(EXIT_FAILURE);
							}

							for (size_t m = 0; m < tmp.size(); m++) {
								if (m == 0) {
									tmp[m].op = VT_MOVETO;
								} else {
									tmp[m].op = VT_LINETO;
								}

								g[k + m] = tmp[m];
							}
						}

						// Now peel off each set of segments from one necessary point to the next
						// into an "arc" as in TopoJSON

						for (size_t m = k; m < l; m++) {
							if (!g[m].necessary) {
								fprintf(stderr, "internal error in arc building\n");
								exit(EXIT_FAILURE);
							}

							drawvec arc;
							size_t n;
							for (n = m; n < l; n++) {
								arc.push_back(g[n]);
								if (n > m && g[n].necessary) {
									break;
								}
							}

							auto f = arcs.find(arc);
							if (f == arcs.end()) {
								drawvec arc2 = reverse_subring(arc);

								auto f2 = arcs.find(arc2);
								if (f2 == arcs.end()) {
									// Add new arc
									size_t added = arcs.size() + 1;
									arcs.insert(std::pair<drawvec, size_t>(arc, added));
									partials[i].arc_polygon.push_back(added);
									merge_candidates.insert(std::pair<ssize_t, size_t>(added, i));
								} else {
									partials[i].arc_polygon.push_back(-(ssize_t) f2->second);
									merge_candidates.insert(std::pair<ssize_t, size_t>(-(ssize_t) f2->second, i));
								}
							} else {
								partials[i].arc_polygon.push_back(f->second);
								merge_candidates.insert(std::pair<ssize_t, size_t>(f->second, i));
							}

							m = n - 1;
						}

						partials[i].arc_polygon.push_back(0);

						k = l - 1;
					}
				}
			}
		}
	}

	// Simplify each arc

	std::vector<drawvec> simplified_arcs;

	size_t count = 0;
	for (auto ai = arcs.begin(); ai != arcs.end(); ++ai) {
		if (simplified_arcs.size() < ai->second + 1) {
			simplified_arcs.resize(ai->second + 1);
		}

		drawvec dv = ai->first;
		for (size_t i = 0; i < dv.size(); i++) {
			if (i == 0) {
				dv[i].op = VT_MOVETO;
			} else {
				dv[i].op = VT_LINETO;
			}
		}
		if (!(prevent[P_SIMPLIFY] || (z == maxzoom && prevent[P_SIMPLIFY_LOW]) || (z < maxzoom && additional[A_GRID_LOW_ZOOMS]))) {
			simplified_arcs[ai->second] = simplify_lines(dv, z, line_detail, !(prevent[P_CLIPPING] || prevent[P_DUPLICATION]), simplification, 4, drawvec());
		} else {
			simplified_arcs[ai->second] = dv;
		}
		count++;
	}

	// If necessary, merge some adjacent polygons into some other polygons

	struct merge_order {
		ssize_t edge = 0;
		unsigned long long gap = 0;
		size_t p1 = 0;
		size_t p2 = 0;

		bool operator<(const merge_order &m) const {
			return gap < m.gap;
		}
	};
	std::vector<merge_order> order;

	for (ssize_t i = 0; i < (ssize_t) simplified_arcs.size(); i++) {
		auto r1 = merge_candidates.equal_range(i);
		for (auto r1i = r1.first; r1i != r1.second; ++r1i) {
			auto r2 = merge_candidates.equal_range(-i);
			for (auto r2i = r2.first; r2i != r2.second; ++r2i) {
				if (r1i->second != r2i->second) {
					merge_order mo;
					mo.edge = i;
					if (partials[r1i->second].index > partials[r2i->second].index) {
						mo.gap = partials[r1i->second].index - partials[r2i->second].index;
					} else {
						mo.gap = partials[r2i->second].index - partials[r1i->second].index;
					}
					mo.p1 = r1i->second;
					mo.p2 = r2i->second;
					order.push_back(mo);
				}
			}
		}
	}
	std::sort(order.begin(), order.end());

	size_t merged = 0;
	for (size_t o = 0; o < order.size(); o++) {
		if (merged >= merge_count) {
			break;
		}

		size_t i = order[o].p1;
		while (partials[i].renamed >= 0) {
			i = partials[i].renamed;
		}
		size_t i2 = order[o].p2;
		while (partials[i2].renamed >= 0) {
			i2 = partials[i2].renamed;
		}

		for (size_t j = 0; j < partials[i].arc_polygon.size() && merged < merge_count; j++) {
			if (partials[i].arc_polygon[j] == order[o].edge) {
				{
					// XXX snap links
					if (partials[order[o].p2].arc_polygon.size() > 0) {
						// This has to merge the ring that contains the anti-arc to this arc
						// into the current ring, and then add whatever other rings were in
						// that feature on to the end.
						//
						// This can't be good for keeping parent-child relationships among
						// the rings in order, but Wagyu should sort that out later

						std::vector<ssize_t> additions;
						std::vector<ssize_t> &here = partials[i].arc_polygon;
						std::vector<ssize_t> &other = partials[i2].arc_polygon;

#if 0
						printf("seeking %zd\n", partials[i].arc_polygon[j]);
						printf("before: ");
						for (size_t k = 0; k < here.size(); k++) {
							printf("%zd ", here[k]);
						}
						printf("\n");
						printf("other: ");
						for (size_t k = 0; k < other.size(); k++) {
							printf("%zd ", other[k]);
						}
						printf("\n");
#endif

						for (size_t k = 0; k < other.size(); k++) {
							size_t l;
							for (l = k; l < other.size(); l++) {
								if (other[l] == 0) {
									break;
								}
							}
							if (l >= other.size()) {
								l--;
							}

#if 0
							for (size_t m = k; m <= l; m++) {
								printf("%zd ", other[m]);
							}
							printf("\n");
#endif

							size_t m;
							for (m = k; m <= l; m++) {
								if (other[m] == -partials[i].arc_polygon[j]) {
									break;
								}
							}

							if (m <= l) {
								// Found the shared arc

								here.erase(here.begin() + j);

								size_t off = 0;
								for (size_t n = m + 1; n < l; n++) {
									here.insert(here.begin() + j + off, other[n]);
									off++;
								}
								for (size_t n = k; n < m; n++) {
									here.insert(here.begin() + j + off, other[n]);
									off++;
								}
							} else {
								// Looking at some other ring

								for (size_t n = k; n <= l; n++) {
									additions.push_back(other[n]);
								}
							}

							k = l;
						}

						partials[i2].arc_polygon.clear();
						partials[i2].renamed = i;
						merged++;

						for (size_t k = 0; k < additions.size(); k++) {
							partials[i].arc_polygon.push_back(additions[k]);
						}

#if 0
						printf("after: ");
						for (size_t k = 0; k < here.size(); k++) {
							printf("%zd ", here[k]);
						}
						printf("\n");
#endif

#if 0
						for (size_t k = 0; k + 1 < here.size(); k++) {
							if (here[k] != 0 && here[k + 1] != 0) {
								if (simplified_arcs[here[k + 1]][0] != simplified_arcs[here[k]][simplified_arcs[here[k]].size() - 1]) {
									printf("error from %zd to %zd\n", here[k], here[k + 1]);
								}
							}
						}
#endif
					}
				}
			}
		}
	}

	// Turn the arc representations of the polygons back into standard polygon geometries

	for (size_t i = 0; i < partials.size(); i++) {
		if (partials[i].t == VT_POLYGON) {
			partials[i].geoms.resize(0);
			partials[i].geoms.push_back(drawvec());
			bool at_start = true;
			draw first(-1, 0, 0);

			for (size_t j = 0; j < partials[i].arc_polygon.size(); j++) {
				ssize_t p = partials[i].arc_polygon[j];

				if (p == 0) {
					if (first.op >= 0) {
						partials[i].geoms[0].push_back(first);
						first = draw(-1, 0, 0);
					}
					at_start = true;
				} else if (p > 0) {
					for (size_t k = 0; k + 1 < simplified_arcs[p].size(); k++) {
						if (at_start) {
							partials[i].geoms[0].push_back(draw(VT_MOVETO, simplified_arcs[p][k].x, simplified_arcs[p][k].y));
							first = draw(VT_LINETO, simplified_arcs[p][k].x, simplified_arcs[p][k].y);
						} else {
							partials[i].geoms[0].push_back(draw(VT_LINETO, simplified_arcs[p][k].x, simplified_arcs[p][k].y));
						}
						at_start = 0;
					}
				} else { /* p < 0 */
					for (ssize_t k = simplified_arcs[-p].size() - 1; k > 0; k--) {
						if (at_start) {
							partials[i].geoms[0].push_back(draw(VT_MOVETO, simplified_arcs[-p][k].x, simplified_arcs[-p][k].y));
							first = draw(VT_LINETO, simplified_arcs[-p][k].x, simplified_arcs[-p][k].y);
						} else {
							partials[i].geoms[0].push_back(draw(VT_LINETO, simplified_arcs[-p][k].x, simplified_arcs[-p][k].y));
						}
						at_start = 0;
					}
				}
			}
		}
	}

	if (merged >= merge_count) {
		return true;
	} else {
		return false;
	}
}

unsigned long long choose_mingap(std::vector<unsigned long long> const &indices, double f) {
	unsigned long long bot = ULLONG_MAX;
	unsigned long long top = 0;

	for (size_t i = 0; i < indices.size(); i++) {
		if (i > 0 && indices[i] >= indices[i - 1]) {
			if (indices[i] - indices[i - 1] > top) {
				top = indices[i] - indices[i - 1];
			}
			if (indices[i] - indices[i - 1] < bot) {
				bot = indices[i] - indices[i - 1];
			}
		}
	}

	size_t want = indices.size() * f;
	while (top - bot > 2) {
		unsigned long long guess = bot / 2 + top / 2;
		size_t count = 0;
		unsigned long long prev = 0;

		for (size_t i = 0; i < indices.size(); i++) {
			if (indices[i] - prev >= guess) {
				count++;
				prev = indices[i];
			}
		}

		if (count > want) {
			bot = guess;
		} else if (count < want) {
			top = guess;
		} else {
			return guess;
		}
	}

	return top;
}

long long choose_minextent(std::vector<long long> &extents, double f) {
	std::sort(extents.begin(), extents.end());
	return extents[(extents.size() - 1) * (1 - f)];
}

struct write_tile_args {
	struct task *tasks = NULL;
	char *metabase = NULL;
	char *stringpool = NULL;
	int min_detail = 0;
	sqlite3 *outdb = NULL;
	const char *outdir = NULL;
	int buffer = 0;
	const char *fname = NULL;
	FILE **geomfile = NULL;
	double todo = 0;
	std::atomic<long long> *along = NULL;
	double gamma = 0;
	double gamma_out = 0;
	int child_shards = 0;
	int *geomfd = NULL;
	off_t *geom_size = NULL;
	std::atomic<unsigned> *midx = NULL;
	std::atomic<unsigned> *midy = NULL;
	int maxzoom = 0;
	int minzoom = 0;
	int full_detail = 0;
	int low_detail = 0;
	double simplification = 0;
	std::atomic<long long> *most = NULL;
	long long *meta_off = NULL;
	long long *pool_off = NULL;
	unsigned *initial_x = NULL;
	unsigned *initial_y = NULL;
	std::atomic<int> *running = NULL;
	int err = 0;
	std::vector<std::map<std::string, layermap_entry>> *layermaps = NULL;
	std::vector<std::vector<std::string>> *layer_unmaps = NULL;
	size_t pass = 0;
	size_t passes = 0;
	unsigned long long mingap = 0;
	unsigned long long mingap_out = 0;
	long long minextent = 0;
	long long minextent_out = 0;
	double fraction = 0;
	double fraction_out = 0;
	const char *prefilter = NULL;
	const char *postfilter = NULL;
	std::map<std::string, attribute_op> const *attribute_accum = NULL;
	bool still_dropping = false;
	int wrote_zoom = 0;
	size_t tiling_seg = 0;
	struct json_object *filter = NULL;
};

bool clip_to_tile(serial_feature &sf, int z, long long buffer) {
	int quick = quick_check(sf.bbox, z, buffer);

	if (z == 0) {
		if (sf.bbox[0] <= (1LL << 32) * buffer / 256 || sf.bbox[2] >= (1LL << 32) - ((1LL << 32) * buffer / 256)) {
			// If the geometry extends off the edge of the world, concatenate on another copy
			// shifted by 360 degrees, and then make sure both copies get clipped down to size.

			size_t n = sf.geometry.size();

			if (sf.bbox[0] <= (1LL << 32) * buffer / 256) {
				for (size_t i = 0; i < n; i++) {
					sf.geometry.push_back(draw(sf.geometry[i].op, sf.geometry[i].x + (1LL << 32), sf.geometry[i].y));
				}
			}

			if (sf.bbox[2] >= (1LL << 32) - ((1LL << 32) * buffer / 256)) {
				for (size_t i = 0; i < n; i++) {
					sf.geometry.push_back(draw(sf.geometry[i].op, sf.geometry[i].x - (1LL << 32), sf.geometry[i].y));
				}
			}

			sf.bbox[0] = 0;
			sf.bbox[2] = 1LL << 32;

			quick = -1;
		}
	}

	if (quick == 0) {
		return true;
	}

	// Can't accept the quick check if guaranteeing no duplication, since the
	// overlap might have been in the buffer.
	if (quick != 1 || prevent[P_DUPLICATION]) {
		drawvec clipped;

		// Do the clipping, even if we are going to include the whole feature,
		// so that we can know whether the feature itself, or only the feature's
		// bounding box, touches the tile.

		if (sf.t == VT_LINE) {
			clipped = clip_lines(sf.geometry, z, buffer);
		}
		if (sf.t == VT_POLYGON) {
			clipped = simple_clip_poly(sf.geometry, z, buffer);
		}
		if (sf.t == VT_POINT) {
			clipped = clip_point(sf.geometry, z, buffer);
		}

		clipped = remove_noop(clipped, sf.t, 0);

		// Must clip at z0 even if we don't want clipping, to handle features
		// that are duplicated across the date line

		if (prevent[P_DUPLICATION] && z != 0) {
			if (point_within_tile((sf.bbox[0] + sf.bbox[2]) / 2, (sf.bbox[1] + sf.bbox[3]) / 2, z)) {
				// sf.geometry is unchanged
			} else {
				sf.geometry.clear();
			}
		} else if (prevent[P_CLIPPING] && z != 0) {
			if (clipped.size() == 0) {
				sf.geometry.clear();
			} else {
				// sf.geometry is unchanged
			}
		} else {
			sf.geometry = clipped;
		}
	}

	return false;
}

void remove_attributes(serial_feature &sf, std::set<std::string> const &exclude_attributes, const char *stringpool, long long *pool_off) {
	for (ssize_t i = sf.keys.size() - 1; i >= 0; i--) {
		std::string key = stringpool + pool_off[sf.segment] + sf.keys[i] + 1;
		if (exclude_attributes.count(key) > 0) {
			sf.keys.erase(sf.keys.begin() + i);
			sf.values.erase(sf.values.begin() + i);
		}
	}

	for (ssize_t i = sf.full_keys.size() - 1; i >= 0; i--) {
		std::string key = sf.full_keys[i];
		if (exclude_attributes.count(key) > 0) {
			sf.full_keys.erase(sf.full_keys.begin() + i);
			sf.full_values.erase(sf.full_values.begin() + i);
		}
	}
}

serial_feature next_feature(FILE *geoms, std::atomic<long long> *geompos_in, char *metabase, long long *meta_off, int z, unsigned tx, unsigned ty, unsigned *initial_x, unsigned *initial_y, long long *original_features, long long *unclipped_features, int nextzoom, int maxzoom, int minzoom, int max_zoom_increment, size_t pass, size_t passes, std::atomic<long long> *along, long long alongminus, int buffer, int *within, bool *first_time, FILE **geomfile, std::atomic<long long> *geompos, std::atomic<double> *oprogress, double todo, const char *fname, int child_shards, struct json_object *filter, const char *stringpool, long long *pool_off, std::vector<std::vector<std::string>> *layer_unmaps) {
	while (1) {
		serial_feature sf = deserialize_feature(geoms, geompos_in, metabase, meta_off, z, tx, ty, initial_x, initial_y);
		if (sf.t < 0) {
			return sf;
		}

		double progress = floor(((((*geompos_in + *along - alongminus) / (double) todo) + (pass - (2 - passes))) / passes + z) / (maxzoom + 1) * 1000) / 10;
		if (progress >= *oprogress + 0.1) {
			if (!quiet && !quiet_progress && progress_time()) {
				fprintf(stderr, "  %3.1f%%  %d/%u/%u  \r", progress, z, tx, ty);
			}
			*oprogress = progress;
		}

		(*original_features)++;

		if (clip_to_tile(sf, z, buffer)) {
			continue;
		}

		if (sf.geometry.size() > 0) {
			(*unclipped_features)++;
		}

		if (*first_time && pass == 1) { /* only write out the next zoom once, even if we retry */
			if (sf.tippecanoe_maxzoom == -1 || sf.tippecanoe_maxzoom >= nextzoom) {
				rewrite(sf.geometry, z, nextzoom, maxzoom, sf.bbox, tx, ty, buffer, within, geompos, geomfile, fname, sf.t, sf.layer, sf.metapos, sf.feature_minzoom, child_shards, max_zoom_increment, sf.seq, sf.tippecanoe_minzoom, sf.tippecanoe_maxzoom, sf.segment, initial_x, initial_y, sf.keys, sf.values, sf.has_id, sf.id, sf.index, sf.extent);
			}
		}

		if (z < minzoom) {
			continue;
		}

		if (sf.tippecanoe_minzoom != -1 && z < sf.tippecanoe_minzoom) {
			continue;
		}
		if (sf.tippecanoe_maxzoom != -1 && z > sf.tippecanoe_maxzoom) {
			continue;
		}

		if (filter != NULL) {
			std::map<std::string, mvt_value> attributes;
			std::string layername = (*layer_unmaps)[sf.segment][sf.layer];
			std::set<std::string> exclude_attributes;

			for (size_t i = 0; i < sf.keys.size(); i++) {
				std::string key = stringpool + pool_off[sf.segment] + sf.keys[i] + 1;

				serial_val sv;
				sv.type = (stringpool + pool_off[sf.segment])[sf.values[i]];
				sv.s = stringpool + pool_off[sf.segment] + sf.values[i] + 1;

				mvt_value val = stringified_to_mvt_value(sv.type, sv.s.c_str());
				attributes.insert(std::pair<std::string, mvt_value>(key, val));
			}

			for (size_t i = 0; i < sf.full_keys.size(); i++) {
				std::string key = sf.full_keys[i];
				mvt_value val = stringified_to_mvt_value(sf.full_values[i].type, sf.full_values[i].s.c_str());

				attributes.insert(std::pair<std::string, mvt_value>(key, val));
			}

			if (sf.has_id) {
				mvt_value v;
				v.type = mvt_uint;
				v.numeric_value.uint_value = sf.id;

				attributes.insert(std::pair<std::string, mvt_value>("$id", v));
			}

			mvt_value v;
			v.type = mvt_string;

			if (sf.t == mvt_point) {
				v.string_value = "Point";
			} else if (sf.t == mvt_linestring) {
				v.string_value = "LineString";
			} else if (sf.t == mvt_polygon) {
				v.string_value = "Polygon";
			}

			attributes.insert(std::pair<std::string, mvt_value>("$type", v));

			mvt_value v2;
			v2.type = mvt_uint;
			v2.numeric_value.uint_value = z;

			attributes.insert(std::pair<std::string, mvt_value>("$zoom", v2));

			if (!evaluate(attributes, layername, filter, exclude_attributes)) {
				continue;
			}

			if (exclude_attributes.size() > 0) {
				remove_attributes(sf, exclude_attributes, stringpool, pool_off);
			}
		}

		if (sf.tippecanoe_minzoom == -1 && z < sf.feature_minzoom) {
			sf.dropped = true;
		}

		// Remove nulls, now that the expression evaluation filter has run

		for (ssize_t i = (ssize_t) sf.keys.size() - 1; i >= 0; i--) {
			int type = (stringpool + pool_off[sf.segment])[sf.values[i]];

			if (type == mvt_null) {
				sf.keys.erase(sf.keys.begin() + i);
				sf.values.erase(sf.values.begin() + i);
			}
		}

		for (ssize_t i = (ssize_t) sf.full_keys.size() - 1; i >= 0; i--) {
			if (sf.full_values[i].type == mvt_null) {
				sf.full_keys.erase(sf.full_keys.begin() + i);
				sf.full_values.erase(sf.full_values.begin() + i);
			}
		}

		return sf;
	}
}

struct run_prefilter_args {
	FILE *geoms = NULL;
	std::atomic<long long> *geompos_in = NULL;
	char *metabase = NULL;
	long long *meta_off = NULL;
	int z = 0;
	unsigned tx = 0;
	unsigned ty = 0;
	unsigned *initial_x = 0;
	unsigned *initial_y = 0;
	long long *original_features = 0;
	long long *unclipped_features = 0;
	int nextzoom = 0;
	int maxzoom = 0;
	int minzoom = 0;
	int max_zoom_increment = 0;
	size_t pass = 0;
	size_t passes = 0;
	std::atomic<long long> *along = 0;
	long long alongminus = 0;
	int buffer = 0;
	int *within = NULL;
	bool *first_time = NULL;
	FILE **geomfile = NULL;
	std::atomic<long long> *geompos = NULL;
	std::atomic<double> *oprogress = NULL;
	double todo = 0;
	const char *fname = 0;
	int child_shards = 0;
	std::vector<std::vector<std::string>> *layer_unmaps = NULL;
	char *stringpool = NULL;
	long long *pool_off = NULL;
	FILE *prefilter_fp = NULL;
	struct json_object *filter = NULL;
};

void *run_prefilter(void *v) {
	run_prefilter_args *rpa = (run_prefilter_args *) v;
	json_writer state(rpa->prefilter_fp);

	while (1) {
		serial_feature sf = next_feature(rpa->geoms, rpa->geompos_in, rpa->metabase, rpa->meta_off, rpa->z, rpa->tx, rpa->ty, rpa->initial_x, rpa->initial_y, rpa->original_features, rpa->unclipped_features, rpa->nextzoom, rpa->maxzoom, rpa->minzoom, rpa->max_zoom_increment, rpa->pass, rpa->passes, rpa->along, rpa->alongminus, rpa->buffer, rpa->within, rpa->first_time, rpa->geomfile, rpa->geompos, rpa->oprogress, rpa->todo, rpa->fname, rpa->child_shards, rpa->filter, rpa->stringpool, rpa->pool_off, rpa->layer_unmaps);
		if (sf.t < 0) {
			break;
		}

		mvt_layer tmp_layer;
		tmp_layer.extent = 1LL << 32;
		tmp_layer.name = (*(rpa->layer_unmaps))[sf.segment][sf.layer];

		if (sf.t == VT_POLYGON) {
			sf.geometry = close_poly(sf.geometry);
		}

		mvt_feature tmp_feature;
		tmp_feature.type = sf.t;
		tmp_feature.geometry = to_feature(sf.geometry);
		tmp_feature.id = sf.id;
		tmp_feature.has_id = sf.has_id;
		tmp_feature.dropped = sf.dropped;

		// Offset from tile coordinates back to world coordinates
		unsigned sx = 0, sy = 0;
		if (rpa->z != 0) {
			sx = rpa->tx << (32 - rpa->z);
			sy = rpa->ty << (32 - rpa->z);
		}
		for (size_t i = 0; i < tmp_feature.geometry.size(); i++) {
			tmp_feature.geometry[i].x += sx;
			tmp_feature.geometry[i].y += sy;
		}

		decode_meta(sf.keys, sf.values, rpa->stringpool + rpa->pool_off[sf.segment], tmp_layer, tmp_feature);
		tmp_layer.features.push_back(tmp_feature);

		layer_to_geojson(tmp_layer, 0, 0, 0, false, true, false, true, sf.index, sf.seq, sf.extent, true, state);
	}

	if (fclose(rpa->prefilter_fp) != 0) {
		if (errno == EPIPE) {
			static bool warned = false;
			if (!warned) {
				fprintf(stderr, "Warning: broken pipe in prefilter\n");
				warned = true;
			}
		} else {
			perror("fclose output to prefilter");
			exit(EXIT_FAILURE);
		}
	}
	return NULL;
}

void add_tilestats(std::string const &layername, int z, std::vector<std::map<std::string, layermap_entry>> *layermaps, size_t tiling_seg, std::vector<std::vector<std::string>> *layer_unmaps, std::string const &key, serial_val const &val) {
	std::map<std::string, layermap_entry> &layermap = (*layermaps)[tiling_seg];
	if (layermap.count(layername) == 0) {
		layermap_entry lme = layermap_entry(layermap.size());
		lme.minzoom = z;
		lme.maxzoom = z;
		lme.retain = 1;

		layermap.insert(std::pair<std::string, layermap_entry>(layername, lme));

		if (lme.id >= (*layer_unmaps)[tiling_seg].size()) {
			(*layer_unmaps)[tiling_seg].resize(lme.id + 1);
			(*layer_unmaps)[tiling_seg][lme.id] = layername;
		}
	}
	auto fk = layermap.find(layername);
	if (fk == layermap.end()) {
		fprintf(stderr, "Internal error: layer %s not found\n", layername.c_str());
		exit(EXIT_FAILURE);
	}

	type_and_string attrib;
	attrib.type = val.type;
	attrib.string = val.s;

	add_to_file_keys(fk->second.file_keys, key, attrib);
}

void preserve_attribute(attribute_op op, serial_feature &, char *stringpool, long long *pool_off, std::string &key, serial_val &val, partial &p) {
	if (p.need_tilestats.count(key) == 0) {
		p.need_tilestats.insert(key);
	}

	// If the feature being merged into has this key as a metadata reference,
	// promote it to a full_key so it can be modified

	for (size_t i = 0; i < p.keys.size(); i++) {
		if (strcmp(key.c_str(), stringpool + pool_off[p.segment] + p.keys[i] + 1) == 0) {
			serial_val sv;
			sv.s = stringpool + pool_off[p.segment] + p.values[i] + 1;
			sv.type = (stringpool + pool_off[p.segment])[p.values[i]];

			p.full_keys.push_back(key);
			p.full_values.push_back(sv);

			p.keys.erase(p.keys.begin() + i);
			p.values.erase(p.values.begin() + i);

			break;
		}
	}

	for (size_t i = 0; i < p.full_keys.size(); i++) {
		if (key == p.full_keys[i]) {
			switch (op) {
			case op_sum:
				p.full_values[i].s = milo::dtoa_milo(atof(p.full_values[i].s.c_str()) + atof(val.s.c_str()));
				p.full_values[i].type = mvt_double;
				break;

			case op_product:
				p.full_values[i].s = milo::dtoa_milo(atof(p.full_values[i].s.c_str()) * atof(val.s.c_str()));
				p.full_values[i].type = mvt_double;
				break;

			case op_max: {
				double existing = atof(p.full_values[i].s.c_str());
				double maybe = atof(val.s.c_str());
				if (maybe > existing) {
					p.full_values[i].s = val.s.c_str();
					p.full_values[i].type = mvt_double;
				}
				break;
			}

			case op_min: {
				double existing = atof(p.full_values[i].s.c_str());
				double maybe = atof(val.s.c_str());
				if (maybe < existing) {
					p.full_values[i].s = val.s.c_str();
					p.full_values[i].type = mvt_double;
				}
				break;
			}

			case op_mean: {
				auto state = p.attribute_accum_state.find(key);
				if (state == p.attribute_accum_state.end()) {
					accum_state s;
					s.sum = atof(p.full_values[i].s.c_str()) + atof(val.s.c_str());
					s.count = 2;
					p.attribute_accum_state.insert(std::pair<std::string, accum_state>(key, s));

					p.full_values[i].s = milo::dtoa_milo(s.sum / s.count);
				} else {
					state->second.sum += atof(val.s.c_str());
					state->second.count += 1;

					p.full_values[i].s = milo::dtoa_milo(state->second.sum / state->second.count);
				}
				break;
			}

			case op_concat:
				p.full_values[i].s += val.s;
				p.full_values[i].type = mvt_string;
				break;

			case op_comma:
				p.full_values[i].s += std::string(",") + val.s;
				p.full_values[i].type = mvt_string;
				break;
			}
		}
	}
}

void preserve_attributes(std::map<std::string, attribute_op> const *attribute_accum, serial_feature &sf, char *stringpool, long long *pool_off, partial &p) {
	for (size_t i = 0; i < sf.keys.size(); i++) {
		std::string key = stringpool + pool_off[sf.segment] + sf.keys[i] + 1;

		serial_val sv;
		sv.type = (stringpool + pool_off[sf.segment])[sf.values[i]];
		sv.s = stringpool + pool_off[sf.segment] + sf.values[i] + 1;

		auto f = attribute_accum->find(key);
		if (f != attribute_accum->end()) {
			preserve_attribute(f->second, sf, stringpool, pool_off, key, sv, p);
		}
	}
	for (size_t i = 0; i < sf.full_keys.size(); i++) {
		std::string key = sf.full_keys[i];
		serial_val sv = sf.full_values[i];

		auto f = attribute_accum->find(key);
		if (f != attribute_accum->end()) {
			preserve_attribute(f->second, sf, stringpool, pool_off, key, sv, p);
		}
	}
}

bool find_partial(std::vector<partial> &partials, serial_feature &sf, ssize_t &out, std::vector<std::vector<std::string>> *layer_unmaps) {
	for (size_t i = partials.size(); i > 0; i--) {
		if (partials[i - 1].t == sf.t) {
			std::string &layername1 = (*layer_unmaps)[partials[i - 1].segment][partials[i - 1].layer];
			std::string &layername2 = (*layer_unmaps)[sf.segment][sf.layer];

			if (layername1 == layername2) {
				out = i - 1;
				return true;
			}
		}
	}

	return false;
}

static bool line_is_too_small(drawvec const &geometry, int z, int detail) {
	if (geometry.size() == 0) {
		return true;
	}

	long long x = geometry[0].x >> (32 - detail - z);
	long long y = geometry[0].y >> (32 - detail - z);

	for (auto &g : geometry) {
		long long xx = g.x >> (32 - detail - z);
		long long yy = g.y >> (32 - detail - z);

		if (xx != x || yy != y) {
			return false;
		}
	}

	return true;
}

long long write_tile(FILE *geoms, std::atomic<long long> *geompos_in, char *metabase, char *stringpool, int z, unsigned tx, unsigned ty, int detail, int min_detail, sqlite3 *outdb, const char *outdir, int buffer, const char *fname, FILE **geomfile, int minzoom, int maxzoom, double todo, std::atomic<long long> *along, long long alongminus, double gamma, int child_shards, long long *meta_off, long long *pool_off, unsigned *initial_x, unsigned *initial_y, std::atomic<int> *running, double simplification, std::vector<std::map<std::string, layermap_entry>> *layermaps, std::vector<std::vector<std::string>> *layer_unmaps, size_t tiling_seg, size_t pass, size_t passes, unsigned long long mingap, long long minextent, double fraction, const char *prefilter, const char *postfilter, struct json_object *filter, write_tile_args *arg) {
	int line_detail;
	double merge_fraction = 1;
	double mingap_fraction = 1;
	double minextent_fraction = 1;

	static std::atomic<double> oprogress(0);
	long long og = *geompos_in;

	// XXX is there a way to do this without floating point?
	int max_zoom_increment = std::log(child_shards) / std::log(4);
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

	bool has_polygons = false;

	bool first_time = true;
	// This only loops if the tile data didn't fit, in which case the detail
	// goes down and the progress indicator goes backward for the next try.
	for (line_detail = detail; line_detail >= min_detail || line_detail == detail; line_detail--, oprogress = 0) {
		long long count = 0;
		double accum_area = 0;

		double fraction_accum = 0;

		unsigned long long previndex = 0, density_previndex = 0, merge_previndex = 0;
		double scale = (double) (1LL << (64 - 2 * (z + 8)));
		double gap = 0, density_gap = 0;
		double spacing = 0;

		long long original_features = 0;
		long long unclipped_features = 0;

		std::vector<struct partial> partials;
		std::map<std::string, std::vector<coalesce>> layers;
		std::vector<unsigned long long> indices;
		std::vector<long long> extents;
		double coalesced_area = 0;
		drawvec shared_nodes;

		int within[child_shards];
		std::atomic<long long> geompos[child_shards];
		for (size_t i = 0; i < (size_t) child_shards; i++) {
			geompos[i] = 0;
			within[i] = 0;
		}

		if (*geompos_in != og) {
			if (fseek(geoms, og, SEEK_SET) != 0) {
				perror("fseek geom");
				exit(EXIT_FAILURE);
			}
			*geompos_in = og;
		}

		int prefilter_write = -1, prefilter_read = -1;
		pid_t prefilter_pid = 0;
		FILE *prefilter_fp = NULL;
		pthread_t prefilter_writer;
		run_prefilter_args rpa;  // here so it stays in scope until joined
		FILE *prefilter_read_fp = NULL;
		json_pull *prefilter_jp = NULL;

		if (z < minzoom) {
			prefilter = NULL;
			postfilter = NULL;
		}

		if (prefilter != NULL) {
			setup_filter(prefilter, &prefilter_write, &prefilter_read, &prefilter_pid, z, tx, ty);
			prefilter_fp = fdopen(prefilter_write, "w");
			if (prefilter_fp == NULL) {
				perror("freopen prefilter");
				exit(EXIT_FAILURE);
			}

			rpa.geoms = geoms;
			rpa.geompos_in = geompos_in;
			rpa.metabase = metabase;
			rpa.meta_off = meta_off;
			rpa.z = z;
			rpa.tx = tx;
			rpa.ty = ty;
			rpa.initial_x = initial_x;
			rpa.initial_y = initial_y;
			rpa.original_features = &original_features;
			rpa.unclipped_features = &unclipped_features;
			rpa.nextzoom = nextzoom;
			rpa.maxzoom = maxzoom;
			rpa.minzoom = minzoom;
			rpa.max_zoom_increment = max_zoom_increment;
			rpa.pass = pass;
			rpa.passes = passes;
			rpa.along = along;
			rpa.alongminus = alongminus;
			rpa.buffer = buffer;
			rpa.within = within;
			rpa.first_time = &first_time;
			rpa.geomfile = geomfile;
			rpa.geompos = geompos;
			rpa.oprogress = &oprogress;
			rpa.todo = todo;
			rpa.fname = fname;
			rpa.child_shards = child_shards;
			rpa.prefilter_fp = prefilter_fp;
			rpa.layer_unmaps = layer_unmaps;
			rpa.stringpool = stringpool;
			rpa.pool_off = pool_off;
			rpa.filter = filter;

			if (pthread_create(&prefilter_writer, NULL, run_prefilter, &rpa) != 0) {
				perror("pthread_create (prefilter writer)");
				exit(EXIT_FAILURE);
			}

			prefilter_read_fp = fdopen(prefilter_read, "r");
			if (prefilter_read_fp == NULL) {
				perror("fdopen prefilter output");
				exit(EXIT_FAILURE);
			}
			prefilter_jp = json_begin_file(prefilter_read_fp);
		}

		while (1) {
			serial_feature sf;
			ssize_t which_partial = -1;

			if (prefilter == NULL) {
				sf = next_feature(geoms, geompos_in, metabase, meta_off, z, tx, ty, initial_x, initial_y, &original_features, &unclipped_features, nextzoom, maxzoom, minzoom, max_zoom_increment, pass, passes, along, alongminus, buffer, within, &first_time, geomfile, geompos, &oprogress, todo, fname, child_shards, filter, stringpool, pool_off, layer_unmaps);
			} else {
				sf = parse_feature(prefilter_jp, z, tx, ty, layermaps, tiling_seg, layer_unmaps, postfilter != NULL);
			}

			if (sf.t < 0) {
				break;
			}

			if (sf.dropped) {
				if (find_partial(partials, sf, which_partial, layer_unmaps)) {
					preserve_attributes(arg->attribute_accum, sf, stringpool, pool_off, partials[which_partial]);
					continue;
				}
			}

			if (gamma > 0) {
				if (manage_gap(sf.index, &previndex, scale, gamma, &gap) && find_partial(partials, sf, which_partial, layer_unmaps)) {
					preserve_attributes(arg->attribute_accum, sf, stringpool, pool_off, partials[which_partial]);
					continue;
				}
			}

			if (additional[A_CLUSTER_DENSEST_AS_NEEDED] || cluster_distance != 0) {
				indices.push_back(sf.index);
				if ((sf.index < merge_previndex || sf.index - merge_previndex < mingap) && find_partial(partials, sf, which_partial, layer_unmaps)) {
					partials[which_partial].clustered++;

					if (partials[which_partial].t == VT_POINT &&
					    partials[which_partial].geoms.size() == 1 &&
					    partials[which_partial].geoms[0].size() == 1 &&
					    sf.geometry.size() == 1) {
						double x = (double) partials[which_partial].geoms[0][0].x * partials[which_partial].clustered;
						double y = (double) partials[which_partial].geoms[0][0].y * partials[which_partial].clustered;
						x += sf.geometry[0].x;
						y += sf.geometry[0].y;
						partials[which_partial].geoms[0][0].x = x / (partials[which_partial].clustered + 1);
						partials[which_partial].geoms[0][0].y = y / (partials[which_partial].clustered + 1);
					}

					preserve_attributes(arg->attribute_accum, sf, stringpool, pool_off, partials[which_partial]);
					continue;
				}
			} else if (additional[A_DROP_DENSEST_AS_NEEDED]) {
				indices.push_back(sf.index);
				if (sf.index - merge_previndex < mingap && find_partial(partials, sf, which_partial, layer_unmaps)) {
					preserve_attributes(arg->attribute_accum, sf, stringpool, pool_off, partials[which_partial]);
					continue;
				}
			} else if (additional[A_COALESCE_DENSEST_AS_NEEDED]) {
				indices.push_back(sf.index);
				if (sf.index - merge_previndex < mingap && find_partial(partials, sf, which_partial, layer_unmaps)) {
					partials[which_partial].geoms.push_back(sf.geometry);
					coalesced_area += sf.extent;
					preserve_attributes(arg->attribute_accum, sf, stringpool, pool_off, partials[which_partial]);
					continue;
				}
			} else if (additional[A_DROP_SMALLEST_AS_NEEDED]) {
				extents.push_back(sf.extent);
				if (sf.extent + coalesced_area <= minextent && sf.t != VT_POINT && find_partial(partials, sf, which_partial, layer_unmaps)) {
					preserve_attributes(arg->attribute_accum, sf, stringpool, pool_off, partials[which_partial]);
					continue;
				}
			} else if (additional[A_COALESCE_SMALLEST_AS_NEEDED]) {
				extents.push_back(sf.extent);
				if (sf.extent + coalesced_area <= minextent && find_partial(partials, sf, which_partial, layer_unmaps)) {
					partials[which_partial].geoms.push_back(sf.geometry);
					coalesced_area += sf.extent;
					preserve_attributes(arg->attribute_accum, sf, stringpool, pool_off, partials[which_partial]);
					continue;
				}
			}

			if (additional[A_CALCULATE_FEATURE_DENSITY]) {
				// Gamma is always 1 for this calculation so there is a reasonable
				// interpretation when no features are being dropped.
				// The spacing is only calculated if a feature would be retained by
				// that standard, so that duplicates aren't reported as infinitely dense.

				double o_density_previndex = density_previndex;
				if (!manage_gap(sf.index, &density_previndex, scale, 1, &density_gap)) {
					spacing = (sf.index - o_density_previndex) / scale;
				}
			}

			fraction_accum += fraction;
			if (fraction_accum < 1 && find_partial(partials, sf, which_partial, layer_unmaps)) {
				if (additional[A_COALESCE_FRACTION_AS_NEEDED]) {
					partials[which_partial].geoms.push_back(sf.geometry);
					coalesced_area += sf.extent;
				}
				preserve_attributes(arg->attribute_accum, sf, stringpool, pool_off, partials[which_partial]);
				continue;
			}
			fraction_accum -= 1;

			bool reduced = false;
			if (sf.t == VT_POLYGON) {
				if (!prevent[P_TINY_POLYGON_REDUCTION] && !additional[A_GRID_LOW_ZOOMS]) {
					sf.geometry = reduce_tiny_poly(sf.geometry, z, line_detail, &reduced, &accum_area);
				}
				has_polygons = true;
			}
			if (sf.t == VT_POLYGON || sf.t == VT_LINE) {
				if (line_is_too_small(sf.geometry, z, line_detail)) {
					continue;
				}
			}

			if (sf.geometry.size() > 0) {
				if (prevent[P_SIMPLIFY_SHARED_NODES]) {
					for (auto &g : sf.geometry) {
						shared_nodes.push_back(g);
					}
				}

				partial p;
				p.geoms.push_back(sf.geometry);
				p.layer = sf.layer;
				p.t = sf.t;
				p.segment = sf.segment;
				p.original_seq = sf.seq;
				p.reduced = reduced;
				p.z = z;
				p.line_detail = line_detail;
				p.maxzoom = maxzoom;
				p.keys = sf.keys;
				p.values = sf.values;
				p.full_keys = sf.full_keys;
				p.full_values = sf.full_values;
				p.spacing = spacing;
				p.simplification = simplification;
				p.id = sf.id;
				p.has_id = sf.has_id;
				p.index = sf.index;
				p.renamed = -1;
				p.extent = sf.extent;
				p.clustered = 0;
				partials.push_back(p);
			}

			merge_previndex = sf.index;
			coalesced_area = 0;
		}

		{
			drawvec just_shared_nodes;
			std::sort(shared_nodes.begin(), shared_nodes.end());

			for (size_t i = 0; i + 1 < shared_nodes.size(); i++) {
				if (shared_nodes[i] == shared_nodes[i + 1]) {
					just_shared_nodes.push_back(shared_nodes[i]);

					draw d = shared_nodes[i];
					i++;
					while (i + 1 < shared_nodes.size() && shared_nodes[i + 1] == d) {
						i++;
					}
				}
			}

			shared_nodes = just_shared_nodes;
		}

		for (size_t i = 0; i < partials.size(); i++) {
			partial &p = partials[i];

			if (p.clustered > 0) {
				std::string layername = (*layer_unmaps)[p.segment][p.layer];
				serial_val sv, sv2, sv3;

				p.full_keys.push_back("clustered");
				sv.type = mvt_bool;
				sv.s = "true";
				p.full_values.push_back(sv);

				add_tilestats(layername, z, layermaps, tiling_seg, layer_unmaps, "clustered", sv);

				p.full_keys.push_back("point_count");
				sv2.type = mvt_double;
				sv2.s = std::to_string(p.clustered + 1);
				p.full_values.push_back(sv2);

				add_tilestats(layername, z, layermaps, tiling_seg, layer_unmaps, "point_count", sv2);

				p.full_keys.push_back("sqrt_point_count");
				sv3.type = mvt_double;
				sv3.s = std::to_string(round(100 * sqrt(p.clustered + 1)) / 100.0);
				p.full_values.push_back(sv3);

				add_tilestats(layername, z, layermaps, tiling_seg, layer_unmaps, "sqrt_point_count", sv3);
			}

			if (p.need_tilestats.size() > 0) {
				std::string layername = (*layer_unmaps)[p.segment][p.layer];

				for (size_t j = 0; j < p.full_keys.size(); j++) {
					if (p.need_tilestats.count(p.full_keys[j]) > 0) {
						add_tilestats(layername, z, layermaps, tiling_seg, layer_unmaps, p.full_keys[j], p.full_values[j]);
					}
				}
			}
		}

		if (prefilter != NULL) {
			json_end(prefilter_jp);
			if (fclose(prefilter_read_fp) != 0) {
				perror("close output from prefilter");
				exit(EXIT_FAILURE);
			}
			while (1) {
				int stat_loc;
				if (waitpid(prefilter_pid, &stat_loc, 0) < 0) {
					perror("waitpid for prefilter\n");
					exit(EXIT_FAILURE);
				}
				if (WIFEXITED(stat_loc) || WIFSIGNALED(stat_loc)) {
					break;
				}
			}
			void *ret;
			if (pthread_join(prefilter_writer, &ret) != 0) {
				perror("pthread_join prefilter writer");
				exit(EXIT_FAILURE);
			}
		}

		first_time = false;
		bool merge_successful = true;

		if (additional[A_DETECT_SHARED_BORDERS] || (additional[A_MERGE_POLYGONS_AS_NEEDED] && merge_fraction < 1)) {
			merge_successful = find_common_edges(partials, z, line_detail, simplification, maxzoom, merge_fraction);
		}

		int tasks = ceil((double) CPUS / *running);
		if (tasks < 1) {
			tasks = 1;
		}

		pthread_t pthreads[tasks];
		std::vector<partial_arg> args;
		args.resize(tasks);
		for (int i = 0; i < tasks; i++) {
			args[i].task = i;
			args[i].tasks = tasks;
			args[i].partials = &partials;
			args[i].shared_nodes = &shared_nodes;

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

		for (size_t i = 0; i < partials.size(); i++) {
			std::vector<drawvec> &pgeoms = partials[i].geoms;
			signed char t = partials[i].t;
			long long original_seq = partials[i].original_seq;

			// A complex polygon may have been split up into multiple geometries.
			// Break them out into multiple features if necessary.
			for (size_t j = 0; j < pgeoms.size(); j++) {
				if (t == VT_POINT || draws_something(pgeoms[j])) {
					struct coalesce c;

					c.type = t;
					c.index = partials[i].index;
					c.geom = pgeoms[j];
					pgeoms[j].clear();
					c.coalesced = false;
					c.original_seq = original_seq;
					c.stringpool = stringpool + pool_off[partials[i].segment];
					c.keys = partials[i].keys;
					c.values = partials[i].values;
					c.full_keys = partials[i].full_keys;
					c.full_values = partials[i].full_values;
					c.spacing = partials[i].spacing;
					c.id = partials[i].id;
					c.has_id = partials[i].has_id;

					// printf("segment %d layer %lld is %s\n", partials[i].segment, partials[i].layer, (*layer_unmaps)[partials[i].segment][partials[i].layer].c_str());

					std::string layername = (*layer_unmaps)[partials[i].segment][partials[i].layer];
					if (layers.count(layername) == 0) {
						layers.insert(std::pair<std::string, std::vector<coalesce>>(layername, std::vector<coalesce>()));
					}

					auto l = layers.find(layername);
					if (l == layers.end()) {
						fprintf(stderr, "Internal error: couldn't find layer %s\n", layername.c_str());
						fprintf(stderr, "segment %d\n", partials[i].segment);
						fprintf(stderr, "layer %lld\n", partials[i].layer);
						exit(EXIT_FAILURE);
					}
					l->second.push_back(c);
				}
			}
		}

		partials.clear();

		int j;
		for (j = 0; j < child_shards; j++) {
			if (within[j]) {
				serialize_byte(geomfile[j], -2, &geompos[j], fname);
				within[j] = 0;
			}
		}

		for (auto layer_iterator = layers.begin(); layer_iterator != layers.end(); ++layer_iterator) {
			std::vector<coalesce> &layer_features = layer_iterator->second;

			if (additional[A_REORDER]) {
				std::sort(layer_features.begin(), layer_features.end());
			}

			std::vector<coalesce> out;
			if (layer_features.size() > 0) {
				out.push_back(layer_features[0]);
			}
			for (size_t x = 1; x < layer_features.size(); x++) {
				size_t y = out.size() - 1;

#if 0
				if (out.size() > 0 && coalcmp(&layer_features[x], &out[y]) < 0) {
					fprintf(stderr, "\nfeature out of order\n");
				}
#endif

				if (additional[A_COALESCE] && out.size() > 0 && coalcmp(&layer_features[x], &out[y]) == 0) {
					for (size_t g = 0; g < layer_features[x].geom.size(); g++) {
						out[y].geom.push_back(layer_features[x].geom[g]);
					}
					out[y].coalesced = true;
				} else {
					out.push_back(layer_features[x]);
				}
			}

			layer_features = out;

			out.clear();
			for (size_t x = 0; x < layer_features.size(); x++) {
				if (layer_features[x].coalesced && layer_features[x].type == VT_LINE) {
					layer_features[x].geom = remove_noop(layer_features[x].geom, layer_features[x].type, 0);
					layer_features[x].geom = simplify_lines(layer_features[x].geom, 32, 0,
										!(prevent[P_CLIPPING] || prevent[P_DUPLICATION]), simplification, layer_features[x].type == VT_POLYGON ? 4 : 0, shared_nodes);
				}

				if (layer_features[x].type == VT_POLYGON) {
					if (layer_features[x].coalesced) {
						layer_features[x].geom = clean_or_clip_poly(layer_features[x].geom, 0, 0, false);
					}

					layer_features[x].geom = close_poly(layer_features[x].geom);
				}

				if (layer_features[x].geom.size() > 0) {
					out.push_back(layer_features[x]);
				}
			}
			layer_features = out;

			if (prevent[P_INPUT_ORDER]) {
				std::sort(layer_features.begin(), layer_features.end(), preservecmp);
			}
		}

		mvt_tile tile;

		for (auto layer_iterator = layers.begin(); layer_iterator != layers.end(); ++layer_iterator) {
			std::vector<coalesce> &layer_features = layer_iterator->second;

			mvt_layer layer;
			layer.name = layer_iterator->first;
			layer.version = 2;
			layer.extent = 1 << line_detail;

			for (size_t x = 0; x < layer_features.size(); x++) {
				mvt_feature feature;

				if (layer_features[x].type == VT_LINE || layer_features[x].type == VT_POLYGON) {
					layer_features[x].geom = remove_noop(layer_features[x].geom, layer_features[x].type, 0);
				}

				if (layer_features[x].geom.size() == 0) {
					continue;
				}

				feature.type = layer_features[x].type;
				feature.geometry = to_feature(layer_features[x].geom);
				count += layer_features[x].geom.size();
				layer_features[x].geom.clear();

				feature.id = layer_features[x].id;
				feature.has_id = layer_features[x].has_id;

				decode_meta(layer_features[x].keys, layer_features[x].values, layer_features[x].stringpool, layer, feature);
				for (size_t a = 0; a < layer_features[x].full_keys.size(); a++) {
					serial_val sv = layer_features[x].full_values[a];
					mvt_value v = stringified_to_mvt_value(sv.type, sv.s.c_str());
					layer.tag(feature, layer_features[x].full_keys[a], v);
				}

				if (additional[A_CALCULATE_FEATURE_DENSITY]) {
					int glow = 255;
					if (layer_features[x].spacing > 0) {
						glow = (1 / layer_features[x].spacing);
						if (glow > 255) {
							glow = 255;
						}
					}

					mvt_value v;
					v.type = mvt_sint;
					v.numeric_value.sint_value = glow;
					layer.tag(feature, "tippecanoe_feature_density", v);

					serial_val sv;
					sv.type = mvt_double;
					sv.s = std::to_string(glow);

					add_tilestats(layer.name, z, layermaps, tiling_seg, layer_unmaps, "tippecanoe_feature_density", sv);
				}

				layer.features.push_back(feature);
			}

			if (layer.features.size() > 0) {
				tile.layers.push_back(layer);
			}
		}

		if (postfilter != NULL) {
			tile.layers = filter_layers(postfilter, tile.layers, z, tx, ty, layermaps, tiling_seg, layer_unmaps, 1 << line_detail);
		}

		if (z == 0 && unclipped_features < original_features / 2 && clipbboxes.size() == 0) {
			fprintf(stderr, "\n\nMore than half the features were clipped away at zoom level 0.\n");
			fprintf(stderr, "Is your data in the wrong projection? It should be in WGS84/EPSG:4326.\n");
		}

		size_t totalsize = 0;
		for (auto layer_iterator = layers.begin(); layer_iterator != layers.end(); ++layer_iterator) {
			std::vector<coalesce> &layer_features = layer_iterator->second;
			totalsize += layer_features.size();
		}

		double progress = floor(((((*geompos_in + *along - alongminus) / (double) todo) + (pass - (2 - passes))) / passes + z) / (maxzoom + 1) * 1000) / 10;
		if (progress >= oprogress + 0.1) {
			if (!quiet && !quiet_progress && progress_time()) {
				fprintf(stderr, "  %3.1f%%  %d/%u/%u  \r", progress, z, tx, ty);
			}
			oprogress = progress;
		}

		if (totalsize > 0 && tile.layers.size() > 0) {
			if (totalsize > max_tile_features && !prevent[P_FEATURE_LIMIT]) {
				if (!quiet) {
					fprintf(stderr, "tile %d/%u/%u has %zu features, >%zu    \n", z, tx, ty, totalsize, max_tile_features);
				}

				if (has_polygons && additional[A_MERGE_POLYGONS_AS_NEEDED] && merge_fraction > .05 && merge_successful) {
					merge_fraction = merge_fraction * max_tile_features / tile.layers.size() * 0.95;
					if (!quiet) {
						fprintf(stderr, "Going to try merging %0.2f%% of the polygons to make it fit\n", 100 - merge_fraction * 100);
					}
					line_detail++;  // to keep it the same when the loop decrements it
					continue;
				} else if (additional[A_INCREASE_GAMMA_AS_NEEDED] && gamma < 10) {
					if (gamma < 1) {
						gamma = 1;
					} else {
						gamma = gamma * 1.25;
					}

					if (gamma > arg->gamma_out) {
						arg->gamma_out = gamma;
						arg->still_dropping = true;
					}

					if (!quiet) {
						fprintf(stderr, "Going to try gamma of %0.3f to make it fit\n", gamma);
					}
					line_detail++;  // to keep it the same when the loop decrements it
					continue;
				} else if (mingap < ULONG_MAX && (additional[A_DROP_DENSEST_AS_NEEDED] || additional[A_COALESCE_DENSEST_AS_NEEDED] || additional[A_CLUSTER_DENSEST_AS_NEEDED])) {
					mingap_fraction = mingap_fraction * max_tile_features / totalsize * 0.90;
					unsigned long long mg = choose_mingap(indices, mingap_fraction);
					if (mg <= mingap) {
						mg = (mingap + 1) * 1.5;

						if (mg <= mingap) {
							mg = ULONG_MAX;
						}
					}
					mingap = mg;
					if (mingap > arg->mingap_out) {
						arg->mingap_out = mingap;
						arg->still_dropping = true;
					}
					if (!quiet) {
						fprintf(stderr, "Going to try keeping the sparsest %0.2f%% of the features to make it fit\n", mingap_fraction * 100.0);
					}
					line_detail++;
					continue;
				} else if (additional[A_DROP_SMALLEST_AS_NEEDED] || additional[A_COALESCE_SMALLEST_AS_NEEDED]) {
					minextent_fraction = minextent_fraction * max_tile_features / totalsize * 0.90;
					long long m = choose_minextent(extents, minextent_fraction);
					if (m != minextent) {
						minextent = m;
						if (minextent > arg->minextent_out) {
							arg->minextent_out = minextent;
							arg->still_dropping = true;
						}
						if (!quiet) {
							fprintf(stderr, "Going to try keeping the biggest %0.2f%% of the features to make it fit\n", minextent_fraction * 100.0);
						}
						line_detail++;
						continue;
					}
				} else if (totalsize > layers.size() && (prevent[P_DYNAMIC_DROP] || additional[A_DROP_FRACTION_AS_NEEDED] || additional[A_COALESCE_FRACTION_AS_NEEDED])) {
					// The 95% is a guess to avoid too many retries
					// and probably actually varies based on how much duplicated metadata there is

					fraction = fraction * max_tile_features / totalsize * 0.95;
					if (!quiet) {
						fprintf(stderr, "Going to try keeping %0.2f%% of the features to make it fit\n", fraction * 100);
					}
					if ((additional[A_DROP_FRACTION_AS_NEEDED] || additional[A_COALESCE_FRACTION_AS_NEEDED]) && fraction < arg->fraction_out) {
						arg->fraction_out = fraction;
						arg->still_dropping = true;
					} else if (prevent[P_DYNAMIC_DROP]) {
						arg->still_dropping = true;
					}
					line_detail++;  // to keep it the same when the loop decrements it
					continue;
				} else {
					fprintf(stderr, "Try using --drop-fraction-as-needed or --drop-densest-as-needed.\n");
					return -1;
				}
			}

			std::string compressed;
			std::string pbf = tile.encode();

			if (!prevent[P_TILE_COMPRESSION]) {
				compress(pbf, compressed);
			} else {
				compressed = pbf;
			}

			if (compressed.size() > max_tile_size && !prevent[P_KILOBYTE_LIMIT]) {
				if (!quiet) {
					fprintf(stderr, "tile %d/%u/%u size is %lld with detail %d, >%zu    \n", z, tx, ty, (long long) compressed.size(), line_detail, max_tile_size);
				}

				if (has_polygons && additional[A_MERGE_POLYGONS_AS_NEEDED] && merge_fraction > .05 && merge_successful) {
					merge_fraction = merge_fraction * max_tile_size / compressed.size() * 0.95;
					if (!quiet) {
						fprintf(stderr, "Going to try merging %0.2f%% of the polygons to make it fit\n", 100 - merge_fraction * 100);
					}
					line_detail++;  // to keep it the same when the loop decrements it
				} else if (additional[A_INCREASE_GAMMA_AS_NEEDED] && gamma < 10) {
					if (gamma < 1) {
						gamma = 1;
					} else {
						gamma = gamma * 1.25;
					}

					if (gamma > arg->gamma_out) {
						arg->gamma_out = gamma;
						arg->still_dropping = true;
					}

					if (!quiet) {
						fprintf(stderr, "Going to try gamma of %0.3f to make it fit\n", gamma);
					}
					line_detail++;  // to keep it the same when the loop decrements it
				} else if (mingap < ULONG_MAX && (additional[A_DROP_DENSEST_AS_NEEDED] || additional[A_COALESCE_DENSEST_AS_NEEDED] || additional[A_CLUSTER_DENSEST_AS_NEEDED])) {
					mingap_fraction = mingap_fraction * max_tile_size / compressed.size() * 0.90;
					unsigned long long mg = choose_mingap(indices, mingap_fraction);
					if (mg <= mingap) {
						double nmg = (mingap + 1) * 1.5;

						if (nmg <= mingap || nmg > ULONG_MAX) {
							mg = ULONG_MAX;
						} else {
							mg = nmg;

							if (mg <= mingap) {
								mg = ULONG_MAX;
							}
						}
					}
					mingap = mg;
					if (mingap > arg->mingap_out) {
						arg->mingap_out = mingap;
						arg->still_dropping = true;
					}
					if (!quiet) {
						fprintf(stderr, "Going to try keeping the sparsest %0.2f%% of the features to make it fit\n", mingap_fraction * 100.0);
					}
					line_detail++;
				} else if (additional[A_DROP_SMALLEST_AS_NEEDED] || additional[A_COALESCE_SMALLEST_AS_NEEDED]) {
					minextent_fraction = minextent_fraction * max_tile_size / compressed.size() * 0.90;
					long long m = choose_minextent(extents, minextent_fraction);
					if (m != minextent) {
						minextent = m;
						if (minextent > arg->minextent_out) {
							arg->minextent_out = minextent;
							arg->still_dropping = true;
						}
						if (!quiet) {
							fprintf(stderr, "Going to try keeping the biggest %0.2f%% of the features to make it fit\n", minextent_fraction * 100.0);
						}
						line_detail++;
						continue;
					}
				} else if (totalsize > layers.size() && (prevent[P_DYNAMIC_DROP] || additional[A_DROP_FRACTION_AS_NEEDED] || additional[A_COALESCE_FRACTION_AS_NEEDED])) {
					// The 95% is a guess to avoid too many retries
					// and probably actually varies based on how much duplicated metadata there is

					fraction = fraction * max_tile_size / compressed.size() * 0.95;
					if (!quiet) {
						fprintf(stderr, "Going to try keeping %0.2f%% of the features to make it fit\n", fraction * 100);
					}
					if ((additional[A_DROP_FRACTION_AS_NEEDED] || additional[A_COALESCE_FRACTION_AS_NEEDED]) && fraction < arg->fraction_out) {
						arg->fraction_out = fraction;
						arg->still_dropping = true;
					} else if (prevent[P_DYNAMIC_DROP]) {
						arg->still_dropping = true;
					}
					line_detail++;  // to keep it the same when the loop decrements it
				}
			} else {
				if (pass == 1) {
					if (pthread_mutex_lock(&db_lock) != 0) {
						perror("pthread_mutex_lock");
						exit(EXIT_FAILURE);
					}

					if (outdb != NULL) {
						mbtiles_write_tile(outdb, z, tx, ty, compressed.data(), compressed.size());
					} else if (outdir != NULL) {
						dir_write_tile(outdir, z, tx, ty, compressed);
					}

					if (pthread_mutex_unlock(&db_lock) != 0) {
						perror("pthread_mutex_unlock");
						exit(EXIT_FAILURE);
					}
				}

				return count;
			}
		} else {
			return count;
		}
	}

	fprintf(stderr, "could not make tile %d/%u/%u small enough\n", z, tx, ty);
	return -1;
}

struct task {
	int fileno = 0;
	struct task *next = NULL;
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

		FILE *geom = fdopen(arg->geomfd[j], "rb");
		if (geom == NULL) {
			perror("mmap geom");
			exit(EXIT_FAILURE);
		}

		std::atomic<long long> geompos(0);
		long long prevgeom = 0;

		while (1) {
			int z;
			unsigned x, y;

			if (!deserialize_int_io(geom, &z, &geompos)) {
				break;
			}
			deserialize_uint_io(geom, &x, &geompos);
			deserialize_uint_io(geom, &y, &geompos);

			arg->wrote_zoom = z;

			// fprintf(stderr, "%d/%u/%u\n", z, x, y);

			long long len = write_tile(geom, &geompos, arg->metabase, arg->stringpool, z, x, y, z == arg->maxzoom ? arg->full_detail : arg->low_detail, arg->min_detail, arg->outdb, arg->outdir, arg->buffer, arg->fname, arg->geomfile, arg->minzoom, arg->maxzoom, arg->todo, arg->along, geompos, arg->gamma, arg->child_shards, arg->meta_off, arg->pool_off, arg->initial_x, arg->initial_y, arg->running, arg->simplification, arg->layermaps, arg->layer_unmaps, arg->tiling_seg, arg->pass, arg->passes, arg->mingap, arg->minextent, arg->fraction, arg->prefilter, arg->postfilter, arg->filter, arg);

			if (len < 0) {
				int *err = &arg->err;
				*err = z - 1;
				return err;
			}

			if (pthread_mutex_lock(&var_lock) != 0) {
				perror("pthread_mutex_lock");
				exit(EXIT_FAILURE);
			}

			if (z == arg->maxzoom) {
				if (len > *arg->most) {
					*arg->midx = x;
					*arg->midy = y;
					*arg->most = len;
				} else if (len == *arg->most) {
					unsigned long long a = (((unsigned long long) x) << 32) | y;
					unsigned long long b = (((unsigned long long) *arg->midx) << 32) | *arg->midy;

					if (a < b) {
						*arg->midx = x;
						*arg->midy = y;
						*arg->most = len;
					}
				}
			}

			*arg->along += geompos - prevgeom;
			prevgeom = geompos;

			if (pthread_mutex_unlock(&var_lock) != 0) {
				perror("pthread_mutex_unlock");
				exit(EXIT_FAILURE);
			}
		}

		if (arg->pass == 1) {
			// Since the fclose() has closed the underlying file descriptor
			arg->geomfd[j] = -1;
		} else {
			int newfd = dup(arg->geomfd[j]);
			if (newfd < 0) {
				perror("dup geometry");
				exit(EXIT_FAILURE);
			}
			if (lseek(newfd, 0, SEEK_SET) < 0) {
				perror("lseek geometry");
				exit(EXIT_FAILURE);
			}
			arg->geomfd[j] = newfd;
		}

		if (fclose(geom) != 0) {
			perror("close geom");
			exit(EXIT_FAILURE);
		}
	}

	arg->running--;
	return NULL;
}

int traverse_zooms(int *geomfd, off_t *geom_size, char *metabase, char *stringpool, std::atomic<unsigned> *midx, std::atomic<unsigned> *midy, int &maxzoom, int minzoom, sqlite3 *outdb, const char *outdir, int buffer, const char *fname, const char *tmpdir, double gamma, int full_detail, int low_detail, int min_detail, long long *meta_off, long long *pool_off, unsigned *initial_x, unsigned *initial_y, double simplification, std::vector<std::map<std::string, layermap_entry>> &layermaps, const char *prefilter, const char *postfilter, std::map<std::string, attribute_op> const *attribute_accum, struct json_object *filter) {
	last_progress = 0;

	// The existing layermaps are one table per input thread.
	// We need to add another one per *tiling* thread so that it can be
	// safely changed during tiling.
	size_t layermaps_off = layermaps.size();
	for (size_t i = 0; i < CPUS; i++) {
		layermaps.push_back(std::map<std::string, layermap_entry>());
	}

	// Table to map segment and layer number back to layer name
	std::vector<std::vector<std::string>> layer_unmaps;
	for (size_t seg = 0; seg < layermaps.size(); seg++) {
		layer_unmaps.push_back(std::vector<std::string>());

		for (auto a = layermaps[seg].begin(); a != layermaps[seg].end(); ++a) {
			if (a->second.id >= layer_unmaps[seg].size()) {
				layer_unmaps[seg].resize(a->second.id + 1);
			}
			layer_unmaps[seg][a->second.id] = a->first;
		}
	}

	int i;
	for (i = 0; i <= maxzoom; i++) {
		std::atomic<long long> most(0);

		FILE *sub[TEMP_FILES];
		int subfd[TEMP_FILES];
		for (size_t j = 0; j < TEMP_FILES; j++) {
			char geomname[strlen(tmpdir) + strlen("/geom.XXXXXXXX" XSTRINGIFY(INT_MAX)) + 1];
			sprintf(geomname, "%s/geom%zu.XXXXXXXX", tmpdir, j);
			subfd[j] = mkstemp_cloexec(geomname);
			// printf("%s\n", geomname);
			if (subfd[j] < 0) {
				perror(geomname);
				exit(EXIT_FAILURE);
			}
			sub[j] = fopen_oflag(geomname, "wb", O_WRONLY | O_CLOEXEC);
			if (sub[j] == NULL) {
				perror(geomname);
				exit(EXIT_FAILURE);
			}
			unlink(geomname);
		}

		size_t useful_threads = 0;
		long long todo = 0;
		for (size_t j = 0; j < TEMP_FILES; j++) {
			todo += geom_size[j];
			if (geom_size[j] > 0) {
				useful_threads++;
			}
		}

		size_t threads = CPUS;
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
		for (int e = 0; e < 30; e++) {
			if (threads >= (1U << e) && threads < (1U << (e + 1))) {
				threads = 1U << e;
				break;
			}
		}
		if (threads >= (1U << 30)) {
			threads = 1U << 30;
		}
		if (threads < 1) {
			threads = 1;
		}

		// Assign temporary files to threads

		std::vector<struct task> tasks;
		tasks.resize(TEMP_FILES);

		struct dispatch {
			struct task *tasks = NULL;
			long long todo = 0;
			struct dispatch *next = NULL;
		};
		std::vector<struct dispatch> dispatches;
		dispatches.resize(threads);

		struct dispatch *dispatch_head = &dispatches[0];
		for (size_t j = 0; j < threads; j++) {
			dispatches[j].tasks = NULL;
			dispatches[j].todo = 0;
			if (j + 1 < threads) {
				dispatches[j].next = &dispatches[j + 1];
			} else {
				dispatches[j].next = NULL;
			}
		}

		for (size_t j = 0; j < TEMP_FILES; j++) {
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

		int err = INT_MAX;

		size_t start = 1;
		if (additional[A_INCREASE_GAMMA_AS_NEEDED] || additional[A_DROP_DENSEST_AS_NEEDED] || additional[A_COALESCE_DENSEST_AS_NEEDED] || additional[A_CLUSTER_DENSEST_AS_NEEDED] || additional[A_DROP_FRACTION_AS_NEEDED] || additional[A_COALESCE_FRACTION_AS_NEEDED] || additional[A_DROP_SMALLEST_AS_NEEDED] || additional[A_COALESCE_SMALLEST_AS_NEEDED]) {
			start = 0;
		}

		double zoom_gamma = gamma;
		unsigned long long zoom_mingap = ((1LL << (32 - i)) / 256 * cluster_distance) * ((1LL << (32 - i)) / 256 * cluster_distance);
		long long zoom_minextent = 0;
		double zoom_fraction = 1;

		for (size_t pass = start; pass < 2; pass++) {
			pthread_t pthreads[threads];
			std::vector<write_tile_args> args;
			args.resize(threads);
			std::atomic<int> running(threads);
			std::atomic<long long> along(0);

			for (size_t thread = 0; thread < threads; thread++) {
				args[thread].metabase = metabase;
				args[thread].stringpool = stringpool;
				args[thread].min_detail = min_detail;
				args[thread].outdb = outdb;  // locked with db_lock
				args[thread].outdir = outdir;
				args[thread].buffer = buffer;
				args[thread].fname = fname;
				args[thread].geomfile = sub + thread * (TEMP_FILES / threads);
				args[thread].todo = todo;
				args[thread].along = &along;  // locked with var_lock
				args[thread].gamma = zoom_gamma;
				args[thread].gamma_out = zoom_gamma;
				args[thread].mingap = zoom_mingap;
				args[thread].mingap_out = zoom_mingap;
				args[thread].minextent = zoom_minextent;
				args[thread].minextent_out = zoom_minextent;
				args[thread].fraction = zoom_fraction;
				args[thread].fraction_out = zoom_fraction;
				args[thread].child_shards = TEMP_FILES / threads;
				args[thread].simplification = simplification;

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
				args[thread].layermaps = &layermaps;
				args[thread].layer_unmaps = &layer_unmaps;
				args[thread].tiling_seg = thread + layermaps_off;
				args[thread].prefilter = prefilter;
				args[thread].postfilter = postfilter;
				args[thread].attribute_accum = attribute_accum;
				args[thread].filter = filter;

				args[thread].tasks = dispatches[thread].tasks;
				args[thread].running = &running;
				args[thread].pass = pass;
				args[thread].passes = 2 - start;
				args[thread].wrote_zoom = -1;
				args[thread].still_dropping = false;

				if (pthread_create(&pthreads[thread], NULL, run_thread, &args[thread]) != 0) {
					perror("pthread_create");
					exit(EXIT_FAILURE);
				}
			}

			for (size_t thread = 0; thread < threads; thread++) {
				void *retval;

				if (pthread_join(pthreads[thread], &retval) != 0) {
					perror("pthread_join");
				}

				if (retval != NULL) {
					err = *((int *) retval);
				}

				if (args[thread].gamma_out > zoom_gamma) {
					zoom_gamma = args[thread].gamma_out;
				}
				if (args[thread].mingap_out > zoom_mingap) {
					zoom_mingap = args[thread].mingap_out;
				}
				if (args[thread].minextent_out > zoom_minextent) {
					zoom_minextent = args[thread].minextent_out;
				}
				if (args[thread].fraction_out < zoom_fraction) {
					zoom_fraction = args[thread].fraction_out;
				}

				// Zoom counter might be lower than reality if zooms are being skipped
				if (args[thread].wrote_zoom > i) {
					i = args[thread].wrote_zoom;
				}

				if (additional[A_EXTEND_ZOOMS] && i == maxzoom && args[thread].still_dropping && maxzoom < MAX_ZOOM) {
					maxzoom++;
				}
			}
		}

		for (size_t j = 0; j < TEMP_FILES; j++) {
			// Can be < 0 if there is only one source file, at z0
			if (geomfd[j] >= 0) {
				if (close(geomfd[j]) != 0) {
					perror("close geom");
					exit(EXIT_FAILURE);
				}
			}
			if (fclose(sub[j]) != 0) {
				perror("close subfile");
				exit(EXIT_FAILURE);
			}

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

	for (size_t j = 0; j < TEMP_FILES; j++) {
		// Can be < 0 if there is only one source file, at z0
		if (geomfd[j] >= 0) {
			if (close(geomfd[j]) != 0) {
				perror("close geom");
				exit(EXIT_FAILURE);
			}
		}
	}

	if (!quiet) {
		fprintf(stderr, "\n");
	}
	return maxzoom;
}
