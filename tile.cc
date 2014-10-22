#include <iostream>
#include <fstream>
#include <string>
#include <stack>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <unistd.h>
#include <zlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <sqlite3.h>
#include "vector_tile.pb.h"

extern "C" {
	#include "tile.h"
	#include "pool.h"
	#include "clip.h"
	#include "mbtiles.h"
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

struct draw {
	int op;
	long long x;
	long long y;
	int necessary;

	draw(int op, long long x, long long y) {
		this->op = op;
		this->x = x;
		this->y = y;
	}

	draw() { }
};

typedef std::vector<draw> drawvec;

drawvec decode_feature(char **meta, int z, unsigned tx, unsigned ty, int detail) {
	drawvec out;

	while (1) {
		draw d;

		deserialize_int(meta, &d.op);
		if (d.op == VT_END) {
			break;
		}

		if (d.op == VT_MOVETO || d.op == VT_LINETO) {
			int wx, wy;
			deserialize_int(meta, &wx);
			deserialize_int(meta, &wy);

			long long wwx = (unsigned) wx;
			long long wwy = (unsigned) wy;

			if (z != 0) {
				wwx -= tx << (32 - z);
				wwy -= ty << (32 - z);
			}

			d.x = wwx;
			d.y = wwy;
		}

		out.push_back(d);
	}

	return out;
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

drawvec remove_noop(drawvec geom, int type) {
	// first pass: remove empty linetos

	long long x = 0, y = 0;
	drawvec out;
	unsigned i;

	for (i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_LINETO && geom[i].x == x && geom[i].y == y) {
			continue;
		}

		if (geom[i].op == VT_CLOSEPATH) {
			out.push_back(geom[i]);
		} else { /* moveto or lineto */
			out.push_back(geom[i]);
			x = geom[i].x;
			y = geom[i].y;
		}
	}

	// second pass: remove unused movetos

	geom = out;
	out.resize(0);

	for (i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			if (i + 1 >= geom.size()) {
				continue;
			}

			if (geom[i + 1].op == VT_MOVETO) {
				continue;
			}

			if (geom[i + 1].op == VT_CLOSEPATH) {
				i++; // also remove unused closepath
				continue;
			}
		}

		out.push_back(geom[i]);
	}

	// second pass: remove empty movetos

	if (type == VT_LINE) {
		geom = out;
		out.resize(0);

		for (i = 0; i < geom.size(); i++) {
			if (geom[i].op == VT_MOVETO) {
				if (i > 0 && geom[i - 1].op == VT_LINETO && geom[i - 1].x == geom[i].x && geom[i - 1].y == geom[i].y) {
					continue;
				}
			}

			out.push_back(geom[i]);
		}
	}

	return out;
}

drawvec shrink_lines(drawvec &geom, int z, int detail, int basezoom, long long *here, double droprate) {
	long long res = 200LL << (32 - 8 - z);
	long long portion = res / exp(log(sqrt(droprate)) * (basezoom - z));

	unsigned i;
	drawvec out;

	for (i = 0; i < geom.size(); i++) {
		if (i > 0 && (geom[i - 1].op == VT_MOVETO || geom[i - 1].op == VT_LINETO) && geom[i].op == VT_LINETO) {
			double dx = (geom[i].x - geom[i - 1].x);
			double dy = (geom[i].y - geom[i - 1].y);
			long long d = sqrt(dx * dx + dy * dy);

			long long n;
			long long next = LONG_LONG_MAX;
			for (n = *here; n < *here + d; n = next) {
				int within;

				if (n % res < portion) {
					next = (n / res) * res + portion;
					within = 1;
				} else {
					next = (n / res + 1) * res;
					within = 0;
				}

				if (next > *here + d) {
					next = *here + d;
				}

				//printf("drawing from %lld to %lld in %lld\n", n - *here, next - *here, d);

				double f1 = (n - *here) / (double) d;
				double f2 = (next - *here) / (double) d;

				if (within) {
					out.push_back(draw(VT_MOVETO, geom[i - 1].x + f1 * (geom[i].x - geom[i - 1].x), geom[i - 1].y + f1 * (geom[i].y - geom[i - 1].y)));
					out.push_back(draw(VT_LINETO, geom[i - 1].x + f2 * (geom[i].x - geom[i - 1].x), geom[i - 1].y + f2 * (geom[i].y - geom[i - 1].y)));
				} else {
					out.push_back(draw(VT_MOVETO, geom[i - 1].x + f2 * (geom[i].x - geom[i - 1].x), geom[i - 1].y + f2 * (geom[i].y - geom[i - 1].y)));
				}
			}

			*here += d;
		} else {
			out.push_back(geom[i]);
		}
	}

	return out;
}

void to_tile_scale(drawvec &geom, int z, int detail) {
	unsigned i;

	for (i = 0; i < geom.size(); i++) {
		geom[i].x >>= (32 - detail - z);
		geom[i].y >>= (32 - detail - z);
	}
}

double square_distance_from_line(long long point_x, long long point_y, long long segA_x, long long segA_y, long long segB_x, long long segB_y) {
	double p2x = segB_x - segA_x;
	double p2y = segB_y - segA_y;
	double something = p2x * p2x + p2y * p2y;
	double u = 0 == something ? 0 : ((point_x - segA_x) * p2x + (point_y - segA_y) * p2y) / something;

	if (u > 1) {
		u = 1;
	} else if (u < 0) {
		u = 0;
	}

	double x = segA_x + u * p2x;
	double y = segA_y + u * p2y;

	double dx = x - point_x;
	double dy = y - point_y;

	return dx * dx + dy * dy;
}

// https://github.com/Project-OSRM/osrm-backend/blob/733d1384a40f/Algorithms/DouglasePeucker.cpp
void douglas_peucker(drawvec &geom, int start, int n, double e) {
	e = e * e;
	std::stack<int> recursion_stack;

	{
		int left_border = 0;
		int right_border = 1;
		// Sweep linerarily over array and identify those ranges that need to be checked
		do {
			if (geom[start + right_border].necessary) {
				recursion_stack.push(left_border);
				recursion_stack.push(right_border);
				left_border = right_border;
			}
			++right_border;
		} while (right_border < n);
	}

	while (!recursion_stack.empty()) {
		// pop next element
		int second = recursion_stack.top();
		recursion_stack.pop();
		int first = recursion_stack.top();
		recursion_stack.pop();

		double max_distance = -1;
		int farthest_element_index = second;

		// find index idx of element with max_distance
		int i;
		for (i = first + 1; i < second; i++) {
			double temp_dist = square_distance_from_line(geom[start + i].x, geom[start + i].y,
				geom[start + first].x, geom[start + first].y,
				geom[start + second].x, geom[start + second].y);

			double distance = fabs(temp_dist);

			if (distance > e && distance > max_distance) {
				farthest_element_index = i;
				max_distance = distance;
			}
		}

		if (max_distance > e) {
			// mark idx as necessary
			geom[start + farthest_element_index].necessary = 1;

			if (1 < farthest_element_index - first) {
				recursion_stack.push(first);
				recursion_stack.push(farthest_element_index);
			}
			if (1 < second - farthest_element_index) {
				recursion_stack.push(farthest_element_index);
				recursion_stack.push(second);
			}
		}
	}
}

static bool inside(draw d, int edge, long long area) {
	long long clip_buffer = area / 64;

	switch (edge) {
		case 0: // top
			return d.y > -clip_buffer;

		case 1: // right
			return d.x < area + clip_buffer;

		case 2: // bottom
			return d.y < area + clip_buffer;

		case 3: // left
			return d.x > -clip_buffer;
	}

	fprintf(stderr, "internal error inside\n");
	exit(EXIT_FAILURE);
}

// http://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
static draw get_line_intersection(draw p0, draw p1, draw p2, draw p3) {
	double s1_x = p1.x - p0.x;
	double s1_y = p1.y - p0.y;
	double s2_x = p3.x - p2.x;
	double s2_y = p3.y - p2.y;

	double s, t;
	s = (-s1_y * (p0.x - p2.x) + s1_x * (p0.y - p2.y)) / (-s2_x * s1_y + s1_x * s2_y);
	t = ( s2_x * (p0.y - p2.y) - s2_y * (p0.x - p2.x)) / (-s2_x * s1_y + s1_x * s2_y);

	return draw(VT_LINETO, p0.x + (t * s1_x), p0.y + (t * s1_y));
}

static draw intersect(draw a, draw b, int edge, long long area) {
	long long clip_buffer = area / 64;

	switch (edge) {
		case 0: // top
			return get_line_intersection(a, b, draw(VT_MOVETO, -clip_buffer, -clip_buffer), draw(VT_MOVETO, area + clip_buffer, -clip_buffer));
			break;

		case 1: // right
			return get_line_intersection(a, b, draw(VT_MOVETO, area + clip_buffer, -clip_buffer), draw(VT_MOVETO, area + clip_buffer, area + clip_buffer));
			break;

		case 2: // bottom 
			return get_line_intersection(a, b, draw(VT_MOVETO, area + clip_buffer, area + clip_buffer), draw(VT_MOVETO, -clip_buffer, area + clip_buffer));
			break;

		case 3: // left
			return get_line_intersection(a, b, draw(VT_MOVETO, -clip_buffer, area + clip_buffer), draw(VT_MOVETO, -clip_buffer, -clip_buffer));
			break;
	}

	fprintf(stderr, "internal error intersecting\n");
	exit(EXIT_FAILURE);
}

// http://en.wikipedia.org/wiki/Sutherland%E2%80%93Hodgman_algorithm
static drawvec clip_poly1(drawvec &geom, int z, int detail) {
	drawvec out = geom;

	long long area = 0xFFFFFFFF;
	if (z != 0) {
		area = 1LL << (32 - z);
	}

	for (int edge = 0; edge < 4; edge++) {
		if (out.size() > 0) {
			drawvec in = out;
			out.resize(0);

			draw S = in[in.size() - 1];

			for (unsigned e = 0; e < in.size(); e++) {
				draw E = in[e];

				if (inside(E, edge, area)) {
					if (!inside(S, edge, area)) {
						out.push_back(intersect(S, E, edge, area));
					}
					out.push_back(E);
				} else if (inside(S, edge, area)) {
					out.push_back(intersect(S, E, edge, area));
				}

				S = E;
			}
		}
	}

	if (out.size() > 0) {
		out[0].op = VT_MOVETO;
		for (unsigned i = 1; i < out.size(); i++) {
			out[i].op = VT_LINETO;
		}
	}

	return out;
}

drawvec clip_poly(drawvec &geom, int z, int detail) {
	if (z == 0) {
		return geom;
	}

	drawvec out;

	for (unsigned i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			unsigned j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op == VT_CLOSEPATH || geom[j].op == VT_MOVETO) {
					break;
				}
			}

			drawvec tmp;
			for (unsigned k = i; k < j; k++) {
				tmp.push_back(geom[k]);
			}
			tmp = clip_poly1(tmp, z, detail);
			for (unsigned k = 0; k < tmp.size(); k++) {
				out.push_back(tmp[k]);
			}

			if (j >= geom.size() || geom[j].op == VT_CLOSEPATH) {
				out.push_back(draw(VT_CLOSEPATH, 0, 0));
				i = j;
			} else {
				i = j - 1;
			}
		} else {
			out.push_back(geom[i]);
		}
	}

	return out;
}

drawvec reduce_tiny_poly(drawvec &geom, int z, int detail, bool *reduced, double *accum_area) {
	drawvec out;
	long long pixel = (1 << (32 - detail - z)) * 3;

	*reduced = true;

	for (unsigned i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			unsigned j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op == VT_CLOSEPATH) {
					break;
				}
			}

			if (j + 1 < geom.size() && geom[j + 1].op == VT_CLOSEPATH) {
				fprintf(stderr, "double closepath\n");
			}

			double area = 0;
			for (unsigned k = i; k < j; k++) {
				area += geom[k].x * geom[i + ((k - i + 1) % (j - i))].y;
				area -= geom[k].y * geom[i + ((k - i + 1) % (j - i))].x;
			}
			area = fabs(area / 2);

			if (area <= pixel * pixel) {
				//printf("area is only %f vs %lld so using square\n", area, pixel * pixel);

				*accum_area += area;
				if (*accum_area > pixel * pixel) {
					// XXX use centroid;

					out.push_back(draw(VT_MOVETO, geom[i].x, geom[i].y));
					out.push_back(draw(VT_LINETO, geom[i].x + pixel, geom[i].y));
					out.push_back(draw(VT_LINETO, geom[i].x + pixel, geom[i].y + pixel));
					out.push_back(draw(VT_LINETO, geom[i].x, geom[i].y + pixel));
					out.push_back(draw(VT_CLOSEPATH, geom[i].x, geom[i].y));

					*accum_area -= pixel * pixel;
				}
			} else {
				//printf("area is %f so keeping instead of %lld\n", area, pixel * pixel);

				for (unsigned k = i; k <= j && k < geom.size(); k++) {
					out.push_back(geom[k]);
				}

				*reduced = false;
			}

			i = j;
		} else {
			fprintf(stderr, "how did we get here with %d?\n", geom[i].op);
			out.push_back(geom[i]);
		}
	}

	return out;
}


drawvec clip_lines(drawvec &geom, int z, int detail) {
	drawvec out;
	unsigned i;

	for (i = 0; i < geom.size(); i++) {
		if (i > 0 && (geom[i - 1].op == VT_MOVETO || geom[i - 1].op == VT_LINETO) && geom[i].op == VT_LINETO) {
			double x1 = geom[i - 1].x;
			double y1 = geom[i - 1].y;

			double x2 = geom[i - 0].x;
			double y2 = geom[i - 0].y;

			unsigned area = 0xFFFFFFFF;
			if (z != 0) {
				area = 1 << (32 - z);
			}

			int c = clip(&x1, &y1, &x2, &y2, 0, 0, area, area);

			if (c > 1) { // clipped
				out.push_back(draw(VT_MOVETO, x1, y1));
				out.push_back(draw(VT_LINETO, x2, y2));
				out.push_back(draw(VT_MOVETO, geom[i].x, geom[i].y));
			} else if (c == 1) { // unchanged
				out.push_back(geom[i]);
			} else { // clipped away entirely
				out.push_back(draw(VT_MOVETO, geom[i].x, geom[i].y));
			}
		} else {
			out.push_back(geom[i]);
		}
	}

	return out;
}

drawvec simplify_lines(drawvec &geom, int z, int detail) {
	int res = 1 << (32 - detail - z);

	unsigned i;
	for (i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			geom[i].necessary = 1;
		} else if (geom[i].op == VT_LINETO) {
			geom[i].necessary = 0;
		} else {
			geom[i].necessary = 1;
		}
	}

	for (i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			unsigned j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op == VT_CLOSEPATH || geom[j].op == VT_MOVETO) {
					break;
				}
			}

			geom[i].necessary = 1;
			geom[j - 1].necessary = 1;

			douglas_peucker(geom, i, j - i, res);
			i = j - 1;
		}
	}

	drawvec out;
	for (i = 0; i < geom.size(); i++) {
		if (geom[i].necessary) {
			out.push_back(geom[i]);
		}
	}

	return out;
}

drawvec reorder_lines(drawvec &geom) {
	// Only reorder simple linestrings with a single moveto

	unsigned i;
	for (i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			if (i != 0) {
				return geom;
			}
		} else if (geom[i].op == VT_LINETO) {
			if (i == 0) {
				return geom;
			}
		} else {
			return geom;
		}
	}

	// Reorder anything that goes up and to the left
	// instead of down and to the right
	// so that it will coalesce better

	unsigned long long l1 = encode(geom[0].x, geom[0].y);
	unsigned long long l2 = encode(geom[geom.size() - 1].x, geom[geom.size() - 1].y);

	if (l1 > l2) {
		drawvec out;
		for (i = 0; i < geom.size(); i++) {
			out.push_back(geom[geom.size() - 1 - i]);
		}
		out[0].op = VT_MOVETO;
		out[out.size() - 1].op = VT_LINETO;
		return out;
	}

	return geom;
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

long long write_tile(struct index *start, struct index *end, char *metabase, unsigned *file_bbox, int z, unsigned tx, unsigned ty, int detail, int basezoom, struct pool *file_keys, char *layername, sqlite3 *outdb, double droprate) {
	int line_detail;
	static bool evaluated = false;

	for (line_detail = detail; line_detail >= MIN_DETAIL; line_detail--) {
		GOOGLE_PROTOBUF_VERIFY_VERSION;

		struct pool keys, values, dup;
		pool_init(&keys, 0);
		pool_init(&values, 0);
		pool_init(&dup, 1);

		double interval = 1;
		double seq = 0;
		long long count = 0;
		long long along = 0;
		double accum_area = 0;

		if (z < basezoom) {
			interval = exp(log(droprate) * (basezoom - z));
		}

		std::vector<coalesce> features;

		struct index *i;
		for (i = start; i < end; i++) {
			int t;

			char *meta = metabase + i->fpos;
			deserialize_int(&meta, &t);

			if (t == VT_POINT) {
				seq++;

				if (seq >= 0) {
					seq -= interval;
				} else {
					continue;
				}
			}

			drawvec geom = decode_feature(&meta, z, tx, ty, line_detail);

			bool reduced = false;
			if (t == VT_POLYGON) {
				geom = reduce_tiny_poly(geom, z, line_detail, &reduced, &accum_area);
			}

			if (t == VT_LINE) {
				geom = clip_lines(geom, z, line_detail);
			}

			if (t == VT_POLYGON) {
				geom = clip_poly(geom, z, line_detail);
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
				struct pool_val *pv = pool_long_long(&dup, &i->fpos, 0);
				if (pv->n == 0) {
					continue;
				}
				pv->n = 0;

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
		pool_free(&dup);

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

