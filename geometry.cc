#include <iostream>
#include <fstream>
#include <string>
#include <stack>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <sqlite3.h>
#include "geometry.hh"

extern "C" {
	#include "tile.h"
	#include "clip.h"
	#include "projection.h"
}

drawvec decode_geometry(char **meta, int z, unsigned tx, unsigned ty, int detail) {
	drawvec out;

	while (1) {
		draw d;

		deserialize_byte(meta, &d.op);
		if (d.op == VT_END) {
			break;
		}

		if (d.op == VT_MOVETO || d.op == VT_LINETO) {
			unsigned wx, wy;
			deserialize_uint(meta, &wx);
			deserialize_uint(meta, &wy);

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

void to_tile_scale(drawvec &geom, int z, int detail) {
	unsigned i;

	for (i = 0; i < geom.size(); i++) {
		geom[i].x >>= (32 - detail - z);
		geom[i].y >>= (32 - detail - z);
	}
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

/* XXX */
#if 0
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
#endif

static bool inside(draw d, int edge, long long area, long long buffer) {
	long long clip_buffer = buffer * area / 256;

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

	double t;
	//s = (-s1_y * (p0.x - p2.x) + s1_x * (p0.y - p2.y)) / (-s2_x * s1_y + s1_x * s2_y);
	t = ( s2_x * (p0.y - p2.y) - s2_y * (p0.x - p2.x)) / (-s2_x * s1_y + s1_x * s2_y);

	return draw(VT_LINETO, p0.x + (t * s1_x), p0.y + (t * s1_y));
}

static draw intersect(draw a, draw b, int edge, long long area, long long buffer) {
	long long clip_buffer = buffer * area / 256;

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
static drawvec clip_poly1(drawvec &geom, int z, int detail, int buffer) {
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

				if (inside(E, edge, area, buffer)) {
					if (!inside(S, edge, area, buffer)) {
						out.push_back(intersect(S, E, edge, area, buffer));
					}
					out.push_back(E);
				} else if (inside(S, edge, area, buffer)) {
					out.push_back(intersect(S, E, edge, area, buffer));
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

drawvec clip_poly(drawvec &geom, int z, int detail, int buffer) {
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
			tmp = clip_poly1(tmp, z, detail, buffer);
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

drawvec clip_point(drawvec &geom, int z, int detail, long long buffer) {
	drawvec out;
	unsigned i;

	long long min = 0;
	long long area = 0xFFFFFFFF;
	if (z != 0) {
		area = 1LL << (32 - z);

		min -= buffer * area / 256;
		area += buffer * area / 256;
	}

	for (i = 0; i < geom.size(); i++) {
		if (geom[i].x >= min && geom[i].y >= min && geom[i].x <= area && geom[i].y <= area) {
			out.push_back(geom[i]);
		}
	}

	return out;
}

drawvec clip_lines(drawvec &geom, int z, int detail, long long buffer) {
	drawvec out;
	unsigned i;

	long long min = 0;
	long long area = 0xFFFFFFFF;
	if (z != 0) {
		area = 1LL << (32 - z);

		min -= buffer * area / 256;
		area += buffer * area / 256;
	}

	for (i = 0; i < geom.size(); i++) {
		if (i > 0 && (geom[i - 1].op == VT_MOVETO || geom[i - 1].op == VT_LINETO) && geom[i].op == VT_LINETO) {
			double x1 = geom[i - 1].x;
			double y1 = geom[i - 1].y;

			double x2 = geom[i - 0].x;
			double y2 = geom[i - 0].y;

			int c = clip(&x1, &y1, &x2, &y2, min, min, area, area);

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

static double square_distance_from_line(long long point_x, long long point_y, long long segA_x, long long segA_y, long long segB_x, long long segB_y) {
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
static void douglas_peucker(drawvec &geom, int start, int n, double e) {
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

	if (geom.size() == 0) {
		return geom;
	}

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
