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

extern "C" {
#include "jsonpull/jsonpull.h"
}

#include "plugin.hpp"

#define CMD_BITS 3

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

int metacmp(size_t m1, const std::vector<size_t> &keys1, const std::vector<size_t> &values1, char *stringpool1, size_t m2, const std::vector<size_t> &keys2, const std::vector<size_t> &values2, char *stringpool2);
int coalindexcmp(const struct coalesce *c1, const struct coalesce *c2);

struct coalesce {
	char *stringpool = NULL;
	std::vector<size_t> keys = std::vector<size_t>();
	std::vector<size_t> values = std::vector<size_t>();
	std::vector<std::string> full_keys = std::vector<std::string>();
	std::vector<serial_val> full_values = std::vector<serial_val>();
	drawvec geom = drawvec();
	unsigned long index = 0;
	size_t original_seq = 0;
	int type = 0;
	size_t m = 0;
	bool coalesced = false;
	double spacing = 0;
	bool has_id = false;
	unsigned long id = 0;

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

	return metacmp(c1->m, c1->keys, c1->values, c1->stringpool, c2->m, c2->keys, c2->values, c2->stringpool);
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

mvt_value retrieve_string(long off, char *stringpool, int *otype) {
	int type = stringpool[off];
	char *s = stringpool + off + 1;

	if (otype != NULL) {
		*otype = type;
	}

	return stringified_to_mvt_value(type, s);
}

void decode_meta(size_t m, std::vector<size_t> const &metakeys, std::vector<size_t> const &metavals, char *stringpool, mvt_layer &layer, mvt_feature &feature) {
	for (size_t i = 0; i < m; i++) {
		int otype;
		mvt_value key = retrieve_string(metakeys[i], stringpool, NULL);
		mvt_value value = retrieve_string(metavals[i], stringpool, &otype);

		layer.tag(feature, key.string_value, value);
	}
}

int metacmp(size_t m1, const std::vector<size_t> &keys1, const std::vector<size_t> &values1, char *stringpool1, size_t m2, const std::vector<size_t> &keys2, const std::vector<size_t> &values2, char *stringpool2) {
	for (size_t i = 0; i < m1 && i < m2; i++) {
		mvt_value key1 = retrieve_string(keys1[i], stringpool1, NULL);
		mvt_value key2 = retrieve_string(keys2[i], stringpool2, NULL);

		if (key1.string_value < key2.string_value) {
			return -1;
		} else if (key1.string_value > key2.string_value) {
			return 1;
		}

		long off1 = values1[i];
		int type1 = stringpool1[off1];
		char *s1 = stringpool1 + off1 + 1;

		long off2 = values2[i];
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

	if (m1 < m2) {
		return -1;
	} else if (m1 > m2) {
		return 1;
	} else {
		return 0;
	}
}

void rewrite(drawvec &geom, int z, int nextzoom, int maxzoom, long *bbox, unsigned tx, unsigned ty, long buffer, bool *within, off_t *geompos, FILE **geomfile, const char *fname, signed char t, size_t layer, long metastart, signed char feature_minzoom, size_t child_shards, int max_zoom_increment, size_t seq, int tippecanoe_minzoom, int tippecanoe_maxzoom, size_t segment, long *initial_x, long *initial_y, size_t m, std::vector<size_t> &metakeys, std::vector<size_t> &metavals, bool has_id, unsigned long id, unsigned long index, long extent) {
	if (geom.size() > 0 && (nextzoom <= maxzoom || additional[A_EXTEND_ZOOMS])) {
		long xo, yo;
		int span = 1 << (nextzoom - z);

		// Get the feature bounding box in pixel (256) coordinates at the child zoom
		// in order to calculate which sub-tiles it can touch including the buffer.
		long bbox2[4];
		for (size_t k = 0; k < 4; k++) {
			// Division instead of right-shift because coordinates can be negative
			bbox2[k] = bbox[k] / (1 << (32 - nextzoom - 8));
		}
		// Decrement the top and left edges so that any features that are
		// touching the edge can potentially be included in the adjacent tiles too.
		bbox2[0] -= buffer + 1;
		bbox2[1] -= buffer + 1;
		bbox2[2] += buffer;
		bbox2[3] += buffer;

		for (size_t k = 0; k < 4; k++) {
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
			geom2.push_back(draw(geom[i].op, (geom[i].x + sx) >> geometry_scale, (geom[i].y + sy) >> geometry_scale));
		}

		for (xo = bbox2[0]; xo <= bbox2[2]; xo++) {
			for (yo = bbox2[1]; yo <= bbox2[3]; yo++) {
				long jx = (long) tx * span + xo;
				long jy = (long) ty * span + yo;

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

				long j = ((jx << max_zoom_increment) |
					  ((jy & ((1U << max_zoom_increment) - 1)))) &
					 (child_shards - 1);

				{
					if (!within[j]) {
						long next_x = (long) tx * span + xo;
						long next_y = (long) ty * span + yo;

						if (next_x < 0 || next_x >= (1L << nextzoom) ||
						    next_y < 0 || next_y >= (1L << nextzoom)) {
							fprintf(stderr, "Internal error: bad tile number %d/%ld/%ld\n", nextzoom, next_x, next_y);
							exit(EXIT_FAILURE);
						}

						serialize_int(geomfile[j], nextzoom, &geompos[j], fname);
						serialize_uint(geomfile[j], (unsigned) next_x, &geompos[j], fname);
						serialize_uint(geomfile[j], (unsigned) next_y, &geompos[j], fname);
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
					sf.m = m;
					sf.feature_minzoom = feature_minzoom;

					if (metastart < 0) {
						for (size_t i = 0; i < m; i++) {
							sf.keys.push_back(metakeys[i]);
							sf.values.push_back(metavals[i]);
						}
					}

					serialize_feature(geomfile[j], &sf, &geompos[j], fname, initial_x[segment] >> geometry_scale, initial_y[segment] >> geometry_scale, true);
				}
			}
		}
	}
}

struct partial {
	std::vector<drawvec> geoms = std::vector<drawvec>();
	std::vector<size_t> keys = std::vector<size_t>();
	std::vector<size_t> values = std::vector<size_t>();
	std::vector<std::string> full_keys = std::vector<std::string>();
	std::vector<serial_val> full_values = std::vector<serial_val>();
	std::vector<ssize_t> arc_polygon = std::vector<ssize_t>();
	size_t layer = 0;
	size_t original_seq = 0;
	size_t index = 0;
	size_t m = 0;
	size_t segment = 0;
	bool reduced = 0;
	int z = 0;
	int line_detail = 0;
	int maxzoom = 0;
	double spacing = 0;
	double simplification = 0;
	signed char t = 0;
	unsigned long id = 0;
	bool has_id = 0;
	ssize_t renamed = 0;
};

struct partial_arg {
	std::vector<struct partial> *partials = NULL;
	size_t task = 0;
	size_t tasks = 0;
};

drawvec revive_polygon(drawvec &geom, double area, int z, int detail) {
	// From area in world coordinates to area in tile coordinates
	long divisor = 1L << (32 - detail - z);
	area /= divisor * divisor;

	if (area == 0) {
		return drawvec();
	}

	long height = (long) ceil(sqrt(area));
	long width = (long) round(area / height);
	if (width == 0) {
		width = 1;
	}

	long sx = 0, sy = 0, n = 0;
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
		drawvec geom = (*partials)[i].geoms[0];  // XXX assumption of a single geometry at the beginning
		(*partials)[i].geoms.clear();		 // avoid keeping two copies in memory
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
					drawvec ngeom = simplify_lines(geom, z, line_detail, !(prevent[P_CLIPPING] || prevent[P_DUPLICATION]), (*partials)[i].simplification, t == VT_POLYGON ? 4 : 0);

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
			geom = reverse_lines(geom);
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

bool manage_gap(unsigned long index, unsigned long *previndex, double scale, double gamma, double *gap) {
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
		long cmp = (long) y1 - s.y1;
		if (cmp == 0) {
			cmp = (long) x1 - s.x1;
		}
		if (cmp == 0) {
			cmp = (long) y2 - s.y2;
		}
		if (cmp == 0) {
			cmp = (long) x2 - s.x2;
		}
		return cmp < 0;
	}
};

struct edgecmp_ring {
	bool operator()(const edge &a, const edge &b) {
		long cmp = (long) a.y1 - b.y1;
		if (cmp == 0) {
			cmp = (long) a.x1 - b.x1;
		}
		if (cmp == 0) {
			cmp = (long) a.y2 - b.y2;
		}
		if (cmp == 0) {
			cmp = (long) a.x2 - b.x2;
		}
		if (cmp == 0) {
			cmp = (long) a.ring - b.ring;
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

static void check_coords_unsigned(draw dv0, draw dv1) {
	if (dv0.x < 0 || dv0.x > UINT_MAX ||
	    dv0.y < 0 || dv0.y > UINT_MAX ||
	    dv1.x < 0 || dv1.x > UINT_MAX ||
	    dv1.y < 0 || dv1.y > UINT_MAX) {
		fprintf(stderr, "Internal error: Out of bounds coordinate %ld,%ld to %ld,%ld\n", dv0.x, dv0.y, dv1.x, dv1.y);
	}
}

void find_common_edges(std::vector<partial> &partials, int z, int line_detail, double simplification, int maxzoom) {
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

						check_coords_unsigned(dv[0], dv[1]);
						if (ring > UINT_MAX) {
							fprintf(stderr, "Internal error: Too many polygon rings %ld\n", ring);
							exit(EXIT_FAILURE);
						}

						edges.push_back(edge((unsigned) dv[0].x, (unsigned) dv[0].y, (unsigned) dv[1].x, (unsigned) dv[1].y, (unsigned) ring));
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
							check_coords_unsigned(left[0], left[1]);
							std::pair<std::vector<edge>::iterator, std::vector<edge>::iterator> e1 = std::equal_range(edges.begin(), edges.end(), edge((unsigned) left[0].x, (unsigned) left[0].y, (unsigned) left[1].x, (unsigned) left[1].y, 0));

							for (size_t k = 0; k < s; k++) {
								drawvec right;

								if (g[a + k] < g[a + k + 1]) {
									right.push_back(g[a + k]);
									right.push_back(g[a + k + 1]);
								} else {
									right.push_back(g[a + k + 1]);
									right.push_back(g[a + k]);
								}

								check_coords_unsigned(right[0], right[1]);
								std::pair<std::vector<edge>::iterator, std::vector<edge>::iterator> e2 = std::equal_range(edges.begin(), edges.end(), edge((unsigned) right[0].x, (unsigned) right[0].y, (unsigned) right[1].x, (unsigned) right[1].y, 0));

								if (right[1] < right[0]) {
									fprintf(stderr, "left misordered\n");
								}

								if (e1.first == e1.second || e2.first == e2.second) {
									fprintf(stderr, "Internal error: polygon edge lookup failed for %ld,%ld to %ld,%ld or %ld,%ld to %ld,%ld\n", left[0].x, left[0].y, left[1].x, left[1].y, right[0].x, right[0].y, right[1].x, right[1].y);
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
						bool has_necessary = false;
						size_t necessary = 0;
						size_t lowest = k;
						size_t l;
						for (l = k + 1; l < g.size(); l++) {
							if (g[l].op != VT_LINETO) {
								break;
							}

							if (g[l].necessary) {
								necessary = l;
								has_necessary = true;
							}
							if (g[l] < g[lowest]) {
								lowest = l;
							}
						}

						if (!has_necessary) {
							necessary = lowest;
							has_necessary = true;
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
							for (size_t m = k; m < necessary; m++) {
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
									partials[i].arc_polygon.push_back((ssize_t) added);
									merge_candidates.insert(std::pair<ssize_t, size_t>(added, i));
								} else {
									partials[i].arc_polygon.push_back(-(ssize_t) f2->second);
									merge_candidates.insert(std::pair<ssize_t, size_t>(-(ssize_t) f2->second, i));
								}
							} else {
								partials[i].arc_polygon.push_back((ssize_t) f->second);
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
			simplified_arcs[ai->second] = simplify_lines(dv, z, line_detail, !(prevent[P_CLIPPING] || prevent[P_DUPLICATION]), simplification, 4);
		} else {
			simplified_arcs[ai->second] = dv;
		}
		count++;
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
					size_t ix = (size_t) p;
					for (size_t k = 0; k + 1 < simplified_arcs[ix].size(); k++) {
						if (at_start) {
							partials[i].geoms[0].push_back(draw(VT_MOVETO, simplified_arcs[ix][k].x, simplified_arcs[ix][k].y));
							first = draw(VT_LINETO, simplified_arcs[ix][k].x, simplified_arcs[ix][k].y);
						} else {
							partials[i].geoms[0].push_back(draw(VT_LINETO, simplified_arcs[ix][k].x, simplified_arcs[ix][k].y));
						}
						at_start = 0;
					}
				} else { /* p < 0 */
					size_t ix = (size_t) -p;
					for (size_t k = simplified_arcs[ix].size() - 1; k > 0; k--) {
						if (at_start) {
							partials[i].geoms[0].push_back(draw(VT_MOVETO, simplified_arcs[ix][k].x, simplified_arcs[ix][k].y));
							first = draw(VT_LINETO, simplified_arcs[ix][k].x, simplified_arcs[ix][k].y);
						} else {
							partials[i].geoms[0].push_back(draw(VT_LINETO, simplified_arcs[ix][k].x, simplified_arcs[ix][k].y));
						}
						at_start = 0;
					}
				}
			}
		}
	}
}

unsigned long choose_mingap(std::vector<unsigned long> const &indices, double f) {
	unsigned long bot = ULONG_MAX;
	unsigned long top = 0;

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

	size_t want = (size_t) floor(indices.size() * f);
	while (top - bot > 2) {
		unsigned long guess = bot / 2 + top / 2;
		size_t count = 0;
		unsigned long prev = 0;

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

long choose_minextent(std::vector<long> &extents, double f) {
	std::sort(extents.begin(), extents.end());
	return extents[(size_t) floor((extents.size() - 1) * (1 - f))];
}

struct write_tile_args {
	struct task *tasks = NULL;
	char *metabase = NULL;
	char *stringpool = NULL;
	int min_detail = 0;
	sqlite3 *outdb = NULL;
	const char *outdir = NULL;
	long buffer = 0;
	const char *fname = NULL;
	FILE **geomfile = NULL;
	double todo = 0;
	volatile long *along = NULL;
	double gamma = 0;
	double gamma_out = 0;
	size_t child_shards = 0;
	int *geomfd = NULL;
	off_t *geom_size = NULL;
	volatile unsigned *midx = NULL;
	volatile unsigned *midy = NULL;
	int maxzoom = 0;
	int minzoom = 0;
	int full_detail = 0;
	int low_detail = 0;
	double simplification = 0;
	volatile long *most = NULL;
	off_t *meta_off = NULL;
	off_t *pool_off = NULL;
	long *initial_x = NULL;
	long *initial_y = NULL;
	volatile size_t *running = NULL;
	int err = 0;
	std::vector<std::map<std::string, layermap_entry>> *layermaps = NULL;
	std::vector<std::vector<std::string>> *layer_unmaps = NULL;
	size_t pass = 0;
	size_t passes = 0;
	unsigned long mingap = 0;
	unsigned long mingap_out = 0;
	long minextent = 0;
	long minextent_out = 0;
	double fraction = 0;
	double fraction_out = 0;
	const char *prefilter = NULL;
	const char *postfilter = NULL;
	bool still_dropping = false;
	bool wrote_zoom = 0;
	size_t tiling_seg = 0;
};

bool clip_to_tile(serial_feature &sf, int z, long buffer) {
	int quick = quick_check(sf.bbox, z, buffer);
	if (quick == 0) {
		return true;
	}

	if (z == 0) {
		if (sf.bbox[0] < 0 || sf.bbox[2] > 1L << 32) {
			// If the geometry extends off the edge of the world, concatenate on another copy
			// shifted by 360 degrees, and then make sure both copies get clipped down to size.

			size_t n = sf.geometry.size();

			if (sf.bbox[0] < 0) {
				for (size_t i = 0; i < n; i++) {
					sf.geometry.push_back(draw(sf.geometry[i].op, sf.geometry[i].x + (1L << 32), sf.geometry[i].y));
				}
			}

			if (sf.bbox[2] > 1L << 32) {
				for (size_t i = 0; i < n; i++) {
					sf.geometry.push_back(draw(sf.geometry[i].op, sf.geometry[i].x - (1L << 32), sf.geometry[i].y));
				}
			}

			sf.bbox[0] = 0;
			sf.bbox[2] = 1L << 32;

			quick = -1;
		}
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

serial_feature next_feature(FILE *geoms, off_t *geompos_in, char *metabase, off_t *meta_off, int z, unsigned tx, unsigned ty, long *initial_x, long *initial_y, long *original_features, long *unclipped_features, int nextzoom, int maxzoom, int minzoom, int max_zoom_increment, size_t pass, size_t passes, volatile long *along, long alongminus, long buffer, bool *within, bool *first_time, FILE **geomfile, off_t *geompos, volatile double *oprogress, double todo, const char *fname, size_t child_shards) {
	while (1) {
		serial_feature sf = deserialize_feature(geoms, geompos_in, metabase, meta_off, z, tx, ty, initial_x, initial_y);
		if (sf.t < 0) {
			return sf;
		}

		double progress = floor(((((*geompos_in + *along - alongminus) / (double) todo) + (pass - (2 - passes))) / passes + z) / (maxzoom + 1) * 1000) / 10;
		if (progress >= *oprogress + 0.1) {
			if (!quiet && !quiet_progress) {
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
				rewrite(sf.geometry, z, nextzoom, maxzoom, sf.bbox, tx, ty, buffer, within, geompos, geomfile, fname, sf.t, sf.layer, sf.metapos, sf.feature_minzoom, child_shards, max_zoom_increment, sf.seq, sf.tippecanoe_minzoom, sf.tippecanoe_maxzoom, sf.segment, initial_x, initial_y, sf.m, sf.keys, sf.values, sf.has_id, sf.id, sf.index, sf.extent);
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
		if (sf.tippecanoe_minzoom == -1 && z < sf.feature_minzoom) {
			continue;
		}

		return sf;
	}
}

struct run_prefilter_args {
	FILE *geoms = NULL;
	off_t *geompos_in = NULL;
	char *metabase = NULL;
	off_t *meta_off = NULL;
	int z = 0;
	unsigned tx = 0;
	unsigned ty = 0;
	long *initial_x = 0;
	long *initial_y = 0;
	long *original_features = 0;
	long *unclipped_features = 0;
	int nextzoom = 0;
	int maxzoom = 0;
	int minzoom = 0;
	int max_zoom_increment = 0;
	size_t pass = 0;
	size_t passes = 0;
	volatile long *along = 0;
	long alongminus = 0;
	long buffer = 0;
	bool *within = NULL;
	bool *first_time = NULL;
	FILE **geomfile = NULL;
	off_t *geompos = NULL;
	volatile double *oprogress = NULL;
	double todo = 0;
	const char *fname = 0;
	size_t child_shards = 0;
	std::vector<std::vector<std::string>> *layer_unmaps = NULL;
	char *stringpool = NULL;
	off_t *pool_off = NULL;
	FILE *prefilter_fp = NULL;
};

void *run_prefilter(void *v) {
	run_prefilter_args *rpa = (run_prefilter_args *) v;

	while (1) {
		serial_feature sf = next_feature(rpa->geoms, rpa->geompos_in, rpa->metabase, rpa->meta_off, rpa->z, rpa->tx, rpa->ty, rpa->initial_x, rpa->initial_y, rpa->original_features, rpa->unclipped_features, rpa->nextzoom, rpa->maxzoom, rpa->minzoom, rpa->max_zoom_increment, rpa->pass, rpa->passes, rpa->along, rpa->alongminus, rpa->buffer, rpa->within, rpa->first_time, rpa->geomfile, rpa->geompos, rpa->oprogress, rpa->todo, rpa->fname, rpa->child_shards);
		if (sf.t < 0) {
			break;
		}

		mvt_layer tmp_layer;
		tmp_layer.extent = 1L << 32;
		tmp_layer.name = (*(rpa->layer_unmaps))[sf.segment][sf.layer];

		if (sf.t == VT_POLYGON) {
			sf.geometry = close_poly(sf.geometry);
		}

		mvt_feature tmp_feature;
		tmp_feature.type = sf.t;
		tmp_feature.geometry = to_feature(sf.geometry);
		tmp_feature.id = sf.id;
		tmp_feature.has_id = sf.has_id;

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

		decode_meta(sf.m, sf.keys, sf.values, rpa->stringpool + rpa->pool_off[sf.segment], tmp_layer, tmp_feature);
		tmp_layer.features.push_back(tmp_feature);

		layer_to_geojson(rpa->prefilter_fp, tmp_layer, 0, 0, 0, false, true, false, sf.index, sf.seq, sf.extent, true);
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

long write_tile(FILE *geoms, off_t *geompos_in, char *metabase, char *stringpool, int z, unsigned tx, unsigned ty, int detail, int min_detail, sqlite3 *outdb, const char *outdir, long buffer, const char *fname, FILE **geomfile, int minzoom, int maxzoom, double todo, volatile long *along, long alongminus, double gamma, size_t child_shards, off_t *meta_off, off_t *pool_off, long *initial_x, long *initial_y, volatile size_t *running, double simplification, std::vector<std::map<std::string, layermap_entry>> *layermaps, std::vector<std::vector<std::string>> *layer_unmaps, size_t tiling_seg, size_t pass, size_t passes, unsigned long mingap, long minextent, double fraction, const char *prefilter, const char *postfilter, write_tile_args *arg) {
	int line_detail;
	double mingap_fraction = 1;
	double minextent_fraction = 1;

	static volatile double oprogress = 0;
	off_t og = *geompos_in;

	// XXX is there a way to do this without floating point?
	int max_zoom_increment = (int) floor(std::log(child_shards) / std::log(4));
	if (child_shards < 4 || max_zoom_increment < 1) {
		fprintf(stderr, "Internal error: %zu shards, max zoom increment %d\n", child_shards, max_zoom_increment);
		exit(EXIT_FAILURE);
	}
	if ((((child_shards - 1) << 1) & child_shards) != child_shards) {
		fprintf(stderr, "Internal error: %zu shards not a power of 2\n", child_shards);
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
		long count = 0;
		double accum_area = 0;

		double fraction_accum = 0;

		unsigned long previndex = 0, density_previndex = 0, merge_previndex = 0;
		double scale = (double) (1L << (64 - 2 * (z + 8)));
		double gap = 0, density_gap = 0;
		double spacing = 0;

		long original_features = 0;
		long unclipped_features = 0;

		std::vector<struct partial> partials;
		std::map<std::string, std::vector<coalesce>> layers;
		std::vector<unsigned long> indices;
		std::vector<long> extents;
		std::vector<serial_feature> coalesced_geometry;

		bool within[child_shards];
		off_t geompos[child_shards];
		for (size_t i = 0; i < child_shards; i++) {
			within[i] = false;
			geompos[i] = 0;
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

			if (prefilter == NULL) {
				sf = next_feature(geoms, geompos_in, metabase, meta_off, z, tx, ty, initial_x, initial_y, &original_features, &unclipped_features, nextzoom, maxzoom, minzoom, max_zoom_increment, pass, passes, along, alongminus, buffer, within, &first_time, geomfile, geompos, &oprogress, todo, fname, child_shards);
			} else {
				sf = parse_feature(prefilter_jp, z, tx, ty, layermaps, tiling_seg, layer_unmaps, postfilter != NULL);
			}

			if (sf.t < 0) {
				break;
			}

			if (gamma > 0) {
				if (manage_gap(sf.index, &previndex, scale, gamma, &gap)) {
					continue;
				}
			}

			double coalesced_area = 0;
			for (size_t i = 0; i < coalesced_geometry.size(); i++) {
				if (coalesced_geometry[i].t == sf.t) {
					coalesced_area += coalesced_geometry[i].extent;
				}
			}

			if (additional[A_DROP_DENSEST_AS_NEEDED]) {
				indices.push_back(sf.index);
				if (sf.index - merge_previndex < mingap) {
					continue;
				}
			}
			if (additional[A_DROP_SMALLEST_AS_NEEDED]) {
				extents.push_back(sf.extent);
				if (sf.extent + coalesced_area <= minextent && sf.t != VT_POINT) {
					continue;
				}
			}
			if (additional[A_COALESCE_SMALLEST_AS_NEEDED]) {
				extents.push_back(sf.extent);
				if (sf.extent + coalesced_area <= minextent) {
					coalesced_geometry.push_back(sf);
					continue;
				}
			}

			if (coalesced_geometry.size() != 0) {
				for (size_t i = coalesced_geometry.size(); i > 0; i--) {
					if (coalesced_geometry[i - 1].t == sf.t && coalesced_geometry[i - 1].layer == sf.layer) {
						for (size_t j = 0; j < coalesced_geometry[i - 1].geometry.size(); j++) {
							sf.geometry.push_back(coalesced_geometry[i - 1].geometry[j]);
						}
						coalesced_geometry.erase(coalesced_geometry.begin() + (ssize_t) i - 1);
					}
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
			if (fraction_accum < 1) {
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

			if (sf.geometry.size() > 0) {
				partial p;
				p.geoms.push_back(sf.geometry);
				p.layer = sf.layer;
				p.m = sf.m;
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
				partials.push_back(p);
			}

			merge_previndex = sf.index;
		}

		// Attach any pieces that were waiting to be coalesced onto some features that did make it.
		for (size_t i = coalesced_geometry.size(); i > 0; i--) {
			for (size_t j = partials.size(); j > 0; j--) {
				if (partials[j - 1].layer == coalesced_geometry[i - 1].layer && partials[j - 1].t == coalesced_geometry[i - 1].t) {
					for (size_t k = 0; k < coalesced_geometry[i - 1].geometry.size(); k++) {
						partials[j - 1].geoms[0].push_back(coalesced_geometry[i - 1].geometry[k]);
					}

					coalesced_geometry.erase(coalesced_geometry.begin() + (ssize_t) i - 1);
					break;
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

		if (additional[A_DETECT_SHARED_BORDERS]) {
			find_common_edges(partials, z, line_detail, simplification, maxzoom);
		}

		size_t tasks = (size_t) ceil((double) CPUS / *running);
		if (tasks < 1) {
			tasks = 1;
		}

		pthread_t pthreads[tasks];
		std::vector<partial_arg> args;
		args.resize(tasks);
		for (size_t i = 0; i < tasks; i++) {
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
			for (size_t i = 0; i < tasks; i++) {
				void *retval;

				if (pthread_join(pthreads[i], &retval) != 0) {
					perror("pthread_join");
				}
			}
		}

		for (size_t i = 0; i < partials.size(); i++) {
			std::vector<drawvec> &pgeoms = partials[i].geoms;
			signed char t = partials[i].t;
			size_t original_seq = partials[i].original_seq;

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
					c.m = partials[i].m;
					c.stringpool = stringpool + pool_off[partials[i].segment];
					c.keys = partials[i].keys;
					c.values = partials[i].values;
					c.full_keys = partials[i].full_keys;
					c.full_values = partials[i].full_values;
					c.spacing = partials[i].spacing;
					c.id = partials[i].id;
					c.has_id = partials[i].has_id;

					// printf("segment %d layer %ld is %s\n", partials[i].segment, partials[i].layer, (*layer_unmaps)[partials[i].segment][partials[i].layer].c_str());

					std::string layername = (*layer_unmaps)[partials[i].segment][partials[i].layer];
					if (layers.count(layername) == 0) {
						layers.insert(std::pair<std::string, std::vector<coalesce>>(layername, std::vector<coalesce>()));
					}

					auto l = layers.find(layername);
					if (l == layers.end()) {
						fprintf(stderr, "Internal error: couldn't find layer %s\n", layername.c_str());
						fprintf(stderr, "segment %zu\n", partials[i].segment);
						fprintf(stderr, "layer %zu\n", partials[i].layer);
						exit(EXIT_FAILURE);
					}
					l->second.push_back(c);
				}
			}
		}

		partials.clear();

		for (size_t j = 0; j < child_shards; j++) {
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

				if (additional[A_COALESCE] && out.size() > 0 && coalcmp(&layer_features[x], &out[y]) == 0 && layer_features[x].type != VT_POINT) {
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
										!(prevent[P_CLIPPING] || prevent[P_DUPLICATION]), simplification, layer_features[x].type == VT_POLYGON ? 4 : 0);
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

				decode_meta(layer_features[x].m, layer_features[x].keys, layer_features[x].values, layer_features[x].stringpool, layer, feature);
				for (size_t a = 0; a < layer_features[x].full_keys.size(); a++) {
					serial_val sv = layer_features[x].full_values[a];
					mvt_value v = stringified_to_mvt_value(sv.type, sv.s.c_str());
					layer.tag(feature, layer_features[x].full_keys[a], v);
				}

				if (additional[A_CALCULATE_FEATURE_DENSITY]) {
					int glow = 255;
					if (layer_features[x].spacing > 0) {
						glow = (int) floor(1 / layer_features[x].spacing);
						if (glow > 255) {
							glow = 255;
						}
					}
					mvt_value v;
					v.type = mvt_sint;
					v.numeric_value.sint_value = glow;
					layer.tag(feature, "tippecanoe_feature_density", v);
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

		if (z == 0 && unclipped_features < original_features / 2) {
			fprintf(stderr, "\n\nMore than half the features were clipped away at zoom level 0.\n");
			fprintf(stderr, "Is your data in the wrong projection? It should be in WGS84/EPSG:4326.\n");
		}

		long totalsize = 0;
		for (auto layer_iterator = layers.begin(); layer_iterator != layers.end(); ++layer_iterator) {
			std::vector<coalesce> &layer_features = layer_iterator->second;
			totalsize += layer_features.size();
		}

		double progress = floor(((((*geompos_in + *along - alongminus) / (double) todo) + (pass - (2 - passes))) / passes + z) / (maxzoom + 1) * 1000) / 10;
		if (progress >= oprogress + 0.1) {
			if (!quiet && !quiet_progress) {
				fprintf(stderr, "  %3.1f%%  %d/%u/%u  \r", progress, z, tx, ty);
			}
			oprogress = progress;
		}

		if (totalsize > 0 && tile.layers.size() > 0) {
			if (totalsize > 200000 && !prevent[P_FEATURE_LIMIT]) {
				fprintf(stderr, "tile %d/%u/%u has %ld features, >200000    \n", z, tx, ty, totalsize);

				if (additional[A_INCREASE_GAMMA_AS_NEEDED] && gamma < 10) {
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
				} else if (additional[A_DROP_DENSEST_AS_NEEDED]) {
					mingap_fraction = mingap_fraction * 200000.0 / totalsize * 0.90;
					unsigned long mg = choose_mingap(indices, mingap_fraction);
					if (mg <= mingap) {
						mg = (unsigned long) floor(mingap * 1.5);
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
					minextent_fraction = minextent_fraction * 200000.0 / totalsize * 0.90;
					long m = choose_minextent(extents, minextent_fraction);
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
				} else if (prevent[P_DYNAMIC_DROP] || additional[A_DROP_FRACTION_AS_NEEDED]) {
					fraction = fraction * 200000 / totalsize * 0.95;
					if (!quiet) {
						fprintf(stderr, "Going to try keeping %0.2f%% of the features to make it fit\n", fraction * 100);
					}
					if (additional[A_DROP_FRACTION_AS_NEEDED] && fraction < arg->fraction_out) {
						arg->fraction_out = fraction;
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
					fprintf(stderr, "tile %d/%u/%u size is %ld with detail %d, >%zu    \n", z, tx, ty, (long) compressed.size(), line_detail, max_tile_size);
				}

				if (additional[A_INCREASE_GAMMA_AS_NEEDED] && gamma < 10) {
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
				} else if (additional[A_DROP_DENSEST_AS_NEEDED]) {
					mingap_fraction = mingap_fraction * max_tile_size / compressed.size() * 0.90;
					unsigned long mg = choose_mingap(indices, mingap_fraction);
					if (mg <= mingap) {
						mg = (unsigned long) floor(mingap * 1.5);
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
					long m = choose_minextent(extents, minextent_fraction);
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
				} else if (prevent[P_DYNAMIC_DROP] || additional[A_DROP_FRACTION_AS_NEEDED]) {
					// The 95% is a guess to avoid too many retries
					// and probably actually varies based on how much duplicated metadata there is

					fraction = fraction * max_tile_size / compressed.size() * 0.95;
					if (!quiet) {
						fprintf(stderr, "Going to try keeping %0.2f%% of the features to make it fit\n", fraction * 100);
					}
					if (additional[A_DROP_FRACTION_AS_NEEDED] && fraction < arg->fraction_out) {
						arg->fraction_out = fraction;
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
	size_t fileno = 0;  // index, not fd
	struct task *next = NULL;
};

void *run_thread(void *vargs) {
	write_tile_args *arg = (write_tile_args *) vargs;
	struct task *task;

	for (task = arg->tasks; task != NULL; task = task->next) {
		size_t j = task->fileno;

		if (arg->geomfd[j] < 0) {
			// only one source file for zoom level 0
			continue;
		}
		if (arg->geom_size[j] == 0) {
			continue;
		}

		// printf("%ld of geom_size\n", (long) geom_size[j]);

		FILE *geom = fdopen(arg->geomfd[j], "rb");
		if (geom == NULL) {
			perror("mmap geom");
			exit(EXIT_FAILURE);
		}

		off_t geompos = 0;
		off_t prevgeom = 0;

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

			long len = write_tile(geom, &geompos, arg->metabase, arg->stringpool, z, x, y, z == arg->maxzoom ? arg->full_detail : arg->low_detail, arg->min_detail, arg->outdb, arg->outdir, arg->buffer, arg->fname, arg->geomfile, arg->minzoom, arg->maxzoom, arg->todo, arg->along, geompos, arg->gamma, arg->child_shards, arg->meta_off, arg->pool_off, arg->initial_x, arg->initial_y, arg->running, arg->simplification, arg->layermaps, arg->layer_unmaps, arg->tiling_seg, arg->pass, arg->passes, arg->mingap, arg->minextent, arg->fraction, arg->prefilter, arg->postfilter, arg);

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
					unsigned long a = (((unsigned long) x) << 32) | y;
					unsigned long b = (((unsigned long) *arg->midx) << 32) | *arg->midy;

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

int traverse_zooms(int *geomfd, off_t *geom_size, char *metabase, char *stringpool, unsigned *midx, unsigned *midy, int &maxzoom, int minzoom, sqlite3 *outdb, const char *outdir, long buffer, const char *fname, const char *tmpdir, double gamma, int full_detail, int low_detail, int min_detail, off_t *meta_off, off_t *pool_off, long *initial_x, long *initial_y, double simplification, std::vector<std::map<std::string, layermap_entry>> &layermaps, const char *prefilter, const char *postfilter) {
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

	for (int i = 0; i <= maxzoom; i++) {
		long most = 0;

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
		long todo = 0;
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
		for (size_t e = 0; e < 30; e++) {
			if (threads >= (1U << e) && threads < (1U << (e + 1))) {
				threads = 1U << e;
				break;
			}
		}
		if (threads >= (1U << 30)) {
			threads = 1U << 30;
		}

		// Assign temporary files to threads

		std::vector<struct task> tasks;
		tasks.resize(TEMP_FILES);

		struct dispatch {
			struct task *tasks = NULL;
			long todo = 0;
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
		if (additional[A_INCREASE_GAMMA_AS_NEEDED] || additional[A_DROP_DENSEST_AS_NEEDED] || additional[A_DROP_FRACTION_AS_NEEDED] || additional[A_DROP_SMALLEST_AS_NEEDED] || additional[A_COALESCE_SMALLEST_AS_NEEDED]) {
			start = 0;
		}

		double zoom_gamma = gamma;
		unsigned long zoom_mingap = 0;
		long zoom_minextent = 0;
		double zoom_fraction = 1;

		for (size_t pass = start; pass < 2; pass++) {
			pthread_t pthreads[threads];
			std::vector<write_tile_args> args;
			args.resize(threads);
			size_t running = threads;
			long along = 0;

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
