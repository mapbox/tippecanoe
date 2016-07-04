#include <iostream>
#include <fstream>
#include <string>
#include <stack>
#include <map>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <unistd.h>
#include <cmath>
#include <limits.h>
#include <sqlite3.h>
#include "geometry.hpp"
#include "clipper/clipper.hpp"
#include "projection.hpp"
#include "serial.hpp"
#include "main.hpp"

static int pnpoly(drawvec &vert, size_t start, size_t nvert, long long testx, long long testy);
static int clip(double *x0, double *y0, double *x1, double *y1, double xmin, double ymin, double xmax, double ymax);

drawvec decode_geometry(FILE *meta, long long *geompos, int z, unsigned tx, unsigned ty, int detail, long long *bbox, unsigned initial_x, unsigned initial_y) {
	drawvec out;

	bbox[0] = LLONG_MAX;
	bbox[1] = LLONG_MAX;
	bbox[2] = LLONG_MIN;
	bbox[3] = LLONG_MIN;

	long long wx = initial_x, wy = initial_y;

	while (1) {
		draw d;

		deserialize_byte_io(meta, &d.op, geompos);
		if (d.op == VT_END) {
			break;
		}

		if (d.op == VT_MOVETO || d.op == VT_LINETO) {
			long long dx, dy;

			deserialize_long_long_io(meta, &dx, geompos);
			deserialize_long_long_io(meta, &dy, geompos);

			wx += dx * (1 << geometry_scale);
			wy += dy * (1 << geometry_scale);

			long long wwx = wx;
			long long wwy = wy;

			if (z != 0) {
				wwx -= tx << (32 - z);
				wwy -= ty << (32 - z);
			}

			if (wwx < bbox[0]) {
				bbox[0] = wwx;
			}
			if (wwy < bbox[1]) {
				bbox[1] = wwy;
			}
			if (wwx > bbox[2]) {
				bbox[2] = wwx;
			}
			if (wwy > bbox[3]) {
				bbox[3] = wwy;
			}

			d.x = wwx;
			d.y = wwy;
		}

		out.push_back(d);
	}

	return out;
}

void to_tile_scale(drawvec &geom, int z, int detail) {
	for (size_t i = 0; i < geom.size(); i++) {
		geom[i].x >>= (32 - detail - z);
		geom[i].y >>= (32 - detail - z);
	}
}

drawvec remove_noop(drawvec geom, int type, int shift) {
	// first pass: remove empty linetos

	long long x = 0, y = 0;
	drawvec out;

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_LINETO && (geom[i].x >> shift) == x && (geom[i].y >> shift) == y) {
			continue;
		}

		if (geom[i].op == VT_CLOSEPATH) {
			out.push_back(geom[i]);
		} else { /* moveto or lineto */
			out.push_back(geom[i]);
			x = geom[i].x >> shift;
			y = geom[i].y >> shift;
		}
	}

	// second pass: remove unused movetos

	if (type != VT_POINT) {
		geom = out;
		out.resize(0);

		for (size_t i = 0; i < geom.size(); i++) {
			if (geom[i].op == VT_MOVETO) {
				if (i + 1 >= geom.size()) {
					continue;
				}

				if (geom[i + 1].op == VT_MOVETO) {
					continue;
				}

				if (geom[i + 1].op == VT_CLOSEPATH) {
					fprintf(stderr, "Shouldn't happen\n");
					i++;  // also remove unused closepath
					continue;
				}
			}

			out.push_back(geom[i]);
		}
	}

	// second pass: remove empty movetos

	if (type == VT_LINE) {
		geom = out;
		out.resize(0);

		for (size_t i = 0; i < geom.size(); i++) {
			if (geom[i].op == VT_MOVETO) {
				if (i > 0 && geom[i - 1].op == VT_LINETO && (geom[i - 1].x >> shift) == (geom[i].x >> shift) && (geom[i - 1].y >> shift) == (geom[i].y >> shift)) {
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

	drawvec out;

	for (size_t i = 0; i < geom.size(); i++) {
		if (i > 0 && (geom[i - 1].op == VT_MOVETO || geom[i - 1].op == VT_LINETO) && geom[i].op == VT_LINETO) {
			double dx = (geom[i].x - geom[i - 1].x);
			double dy = (geom[i].y - geom[i - 1].y);
			long long d = sqrt(dx * dx + dy * dy);

			long long n;
			long long next = LLONG_MAX;
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

double get_area(drawvec &geom, size_t i, size_t j) {
	double area = 0;
	for (size_t k = i; k < j; k++) {
		area += (long double) geom[k].x * (long double) geom[i + ((k - i + 1) % (j - i))].y;
		area -= (long double) geom[k].y * (long double) geom[i + ((k - i + 1) % (j - i))].x;
	}
	area /= 2;
	return area;
}

void reverse_ring(drawvec &geom, size_t start, size_t end) {
	drawvec tmp;

	for (size_t i = start; i < end; i++) {
		tmp.push_back(geom[i]);
	}

	for (unsigned i = start; i < end; i++) {
		geom[i] = tmp[end - 1 - i];
		if (i == start) {
			geom[i].op = VT_MOVETO;
		} else if (i == end - 1) {
			geom[i].op = VT_LINETO;
		}
	}
}

struct ring {
	drawvec data;
	long double area;
	long long parent;
	std::vector<size_t> children;

	ring(drawvec &_data) {
		data = _data;
		area = get_area(_data, 0, _data.size());
		parent = -1;
	}

	bool operator<(const ring &o) const {
		if (std::fabs(this->area) < std::fabs(o.area)) {
			return true;
		} else {
			return false;
		}
	}
};

static void decode_rings(ClipperLib::PolyNode *t, std::vector<ring> &out) {
	// Supposedly outer ring

	ClipperLib::Path p = t->Contour;
	drawvec dv;
	for (size_t i = 0; i < p.size(); i++) {
		dv.push_back(draw((i == 0) ? VT_MOVETO : VT_LINETO, p[i].X, p[i].Y));
	}
	if (p.size() > 0) {
		dv.push_back(draw(VT_LINETO, p[0].X, p[0].Y));
	}
	out.push_back(dv);

	// Supposedly inner rings

	for (int n = 0; n < t->ChildCount(); n++) {
		ClipperLib::Path cp = t->Childs[n]->Contour;
		drawvec ring;
		for (size_t i = 0; i < cp.size(); i++) {
			ring.push_back(draw((i == 0) ? VT_MOVETO : VT_LINETO, cp[i].X, cp[i].Y));
		}
		if (cp.size() > 0) {
			ring.push_back(draw(VT_LINETO, cp[0].X, cp[0].Y));
		}
		out.push_back(ring);
	}

	// Recurse to supposedly outer rings (children of the children)

	for (int n = 0; n < t->ChildCount(); n++) {
		for (int m = 0; m < t->Childs[n]->ChildCount(); m++) {
			decode_rings(t->Childs[n]->Childs[m], out);
		}
	}
}

static void decode_clipped(ClipperLib::PolyNode *t, drawvec &out) {
	// The output of Clipper supposedly produces the outer rings
	// as top level objects, with links to any inner-ring children
	// they may have, each of which then has links to any outer rings
	// that it has, and so on. This doesn't actually work.

	// So instead, we pull out all the rings, sort them by absolute area,
	// and go through them, looking for the
	// smallest parent that contains a point from it, since we are
	// guaranteed that at least one point in the polygon is strictly
	// inside its parent (not on one of its boundary lines).

	std::vector<ring> rings;
	decode_rings(t, rings);
	std::sort(rings.begin(), rings.end());

	for (size_t i = 0; i < rings.size(); i++) {
		for (size_t j = i + 1; j < rings.size(); j++) {
			for (size_t k = 0; k < rings[i].data.size(); k++) {
				if (pnpoly(rings[j].data, 0, rings[j].data.size(), rings[i].data[k].x, rings[i].data[k].y)) {
					rings[i].parent = j;
					rings[j].children.push_back(i);
					goto nextring;
				}
			}
		}
	nextring:;
	}

	// Then reverse the winding order of any rings that turned out
	// to actually be inner when they are outer, or vice versa.
	// (A ring is outer if it has no parent or if its parent is
	// an inner ring.)

	for (size_t ii = rings.size(); ii > 0; ii--) {
		size_t i = ii - 1;

		if (rings[i].parent < 0) {
			if (rings[i].area < 0) {
				rings[i].area = -rings[i].area;
				reverse_ring(rings[i].data, 0, rings[i].data.size());
			}
		} else {
			if ((rings[i].area > 0) == (rings[rings[i].parent].area > 0)) {
				rings[i].area = -rings[i].area;
				reverse_ring(rings[i].data, 0, rings[i].data.size());
			}
		}
	}

	// Then run through the rings again, outputting each outer ring
	// followed by its direct children, and checking to make sure
	// there are no child rings whose parents weren't identified.

	for (size_t ii = rings.size(); ii > 0; ii--) {
		size_t i = ii - 1;

		if (rings[i].area > 0) {
#if 0
			fprintf(stderr, "ring area %Lf at %lld\n", rings[i].area, (long long) out.size());
#endif

			for (size_t j = 0; j < rings[i].data.size(); j++) {
				out.push_back(rings[i].data[j]);
			}

			for (size_t j = 0; j < rings[i].children.size(); j++) {
#if 0
				fprintf(stderr, "ring area %Lf at %lld\n", rings[rings[i].children[j]].area, (long long) out.size());
#endif

				for (size_t k = 0; k < rings[rings[i].children[j]].data.size(); k++) {
					out.push_back(rings[rings[i].children[j]].data[k]);
				}

				rings[rings[i].children[j]].parent = -2;
			}
		} else if (rings[i].parent != -2) {
			fprintf(stderr, "Found ring with child area but no parent %lld\n", (long long) i);
		}
	}
}

static void dump(drawvec &geom) {
	ClipperLib::Clipper clipper(ClipperLib::ioStrictlySimple);

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			size_t j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op != VT_LINETO) {
					break;
				}
			}

			ClipperLib::Path path;
			printf("{ ClipperLib::Path path; ");

			drawvec tmp;
			for (size_t k = i; k < j; k++) {
				printf("path.push_back(IntPoint(%lld,%lld)); ", geom[k].x, geom[k].y);
				path.push_back(ClipperLib::IntPoint(geom[k].x, geom[k].y));
			}

			if (!clipper.AddPath(path, ClipperLib::ptSubject, true)) {
			}
			printf("clipper.AddPath(path, ClipperLib::ptSubject, true); }\n");

			i = j - 1;
		} else {
			fprintf(stderr, "Unexpected operation in polygon %d\n", (int) geom[i].op);
			exit(EXIT_FAILURE);
		}
	}
	printf("clipper.Execute(ClipperLib::ctUnion, clipped));\n");
}

void add_vertical(size_t intermediate, size_t which_end, size_t into, std::vector<drawvec> &segments, bool &again) {
	again = true;
	drawvec dv;
	dv.push_back(segments[intermediate][which_end]);
	dv.push_back(segments[into][1]);
	segments.push_back(dv);
	segments[into][1] = segments[intermediate][which_end];
	fprintf(stderr, "split vertical\n");
}

void add_horizontal(size_t intermediate, size_t which_end, size_t into, std::vector<drawvec> &segments, bool &again) {
	again = true;

	long long x = segments[intermediate][which_end].x;
	long long y = segments[intermediate][0].y +
		      (segments[intermediate][which_end].x - segments[intermediate][0].x) *
			      (segments[intermediate][1].y - segments[intermediate][0].y) /
			      (segments[intermediate][1].x - segments[intermediate][0].x);
	draw d(VT_LINETO, x, y);

	fprintf(stderr, "split horizontal: %lld,%lld in %lld,%lld to %lld,%lld and %lld,%lld to %lld,%lld\n",
		d.x, d.y,
		segments[intermediate][0].x, segments[intermediate][0].y,
		segments[intermediate][1].x, segments[intermediate][1].y,
		segments[into][0].x, segments[into][0].y,
		segments[into][1].x, segments[into][1].y);

	drawvec dv;
	dv.push_back(d);
	dv.push_back(segments[into][1]);
	segments.push_back(dv);
	segments[into][1] = d;
}

// http://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
void check_intersection(std::vector<drawvec> &segments, size_t a, size_t b, bool &again) {
	long long s10_x = segments[a][1].x - segments[a][0].x;
	long long s10_y = segments[a][1].y - segments[a][0].y;
	long long s32_x = segments[b][1].x - segments[b][0].x;
	long long s32_y = segments[b][1].y - segments[b][0].y;

	long long denom = s10_x * s32_y - s32_x * s10_y;

	if (denom == 0) {
		// They are parallel or collinear. Find out if they are collinear.
		// http://www.cpsc.ucalgary.ca/~marina/papers/Segment_intersection.ps

		long long ccw =
			segments[a][0].x * segments[a][1].y +
			segments[a][1].x * segments[b][0].y +
			segments[b][0].x * segments[a][0].y -
			segments[a][0].x * segments[b][0].y -
			segments[a][1].x * segments[a][0].y -
			segments[b][0].x * segments[a][1].y;

		if (ccw == 0) {
#if 0
			fprintf(stderr, "collinear %lld: %lld,%lld to %lld,%lld and %lld,%lld to %lld,%lld\n", ccw,
			       segments[a][0].x, segments[a][0].y,
			       segments[a][1].x, segments[a][1].y,
			       segments[b][0].x, segments[b][0].y,
			       segments[b][1].x, segments[b][1].y);
#endif

			if (segments[a][0].x == segments[a][1].x) {
				// Vertical

				// All of these transformations preserve verticality so we can check multiple cases
				if (segments[b][0].y > segments[a][0].y && segments[b][0].y < segments[a][1].y) {
					// B0 is in A
					add_vertical(b, 0, a, segments, again);
				}
				if (segments[b][1].y > segments[a][0].y && segments[b][1].y < segments[a][1].y) {
					// B1 is in A
					add_vertical(b, 1, a, segments, again);
				}
				if (segments[a][0].y > segments[b][0].y && segments[a][0].y < segments[b][1].y) {
					// A0 is in B
					add_vertical(a, 0, b, segments, again);
				}
				if (segments[a][0].y > segments[b][0].y && segments[a][0].y < segments[b][1].y) {
					// A1 is in B
					add_vertical(a, 1, b, segments, again);
				}
			} else {
				// Horizontal or diagonal

				// Don't check multiples, because rounding may corrupt collinearity
				if (segments[b][0].x > segments[a][0].x && segments[b][0].x < segments[a][1].x) {
					// B0 is in A
					add_horizontal(b, 0, a, segments, again);
				} else if (segments[b][1].x > segments[a][0].x && segments[b][1].x < segments[a][1].x) {
					// B1 is in A
					add_horizontal(b, 1, a, segments, again);
				} else if (segments[a][0].x > segments[b][0].x && segments[a][0].x < segments[b][1].x) {
					// A0 is in B
					add_horizontal(a, 0, b, segments, again);
				} else if (segments[a][0].x > segments[b][0].x && segments[a][0].x < segments[b][1].x) {
					// A1 is in B
					add_horizontal(a, 1, b, segments, again);
				}
			}
		}
	} else {
		// Neither parallel nor collinear, so may intersect at a single point

		long long s02_x = segments[a][0].x - segments[b][0].x;
		long long s02_y = segments[a][0].y - segments[b][0].y;

		double s = (s10_x * s02_y - s10_y * s02_x) / (long double) denom;
		double t = (s32_x * s02_y - s32_y * s02_x) / (long double) denom;

		if ((t >= 0 && t <= 1 && s > 0 && s < 1) ||
		    (t > 0 && t < 1 && s >= 0 && s <= 1)) {
			long long x = round(segments[a][0].x + t * s10_x);
			long long y = round(segments[a][0].y + t * s10_y);
			again = true;

			fprintf(stderr, "intersection %f,%f (%f) / %f,%f (%f) in %lld,%lld to %lld,%lld and %lld,%lld to %lld,%lld\n",
				segments[a][0].x + t * s10_x, segments[a][0].y + t * s10_y, t,
				segments[b][0].x + s * s32_x, segments[b][0].y + s * s32_y, s,
				segments[a][0].x, segments[a][0].y,
				segments[a][1].x, segments[a][1].y,
				segments[b][0].x, segments[b][0].y,
				segments[b][1].x, segments[b][1].y);

			if (t > 0 && t < 1) {
				if ((x != segments[a][0].x || y != segments[a][0].y) && (x != segments[a][1].x || y != segments[a][1].y)) {
					// splitting a
					drawvec dv;
					dv.push_back(draw(VT_MOVETO, x, y));
					dv.push_back(segments[a][1]);
					segments.push_back(dv);
					segments[a][1] = draw(VT_LINETO, x, y);
				}
			}

			if (s > 0 && s < 1) {
				if ((x != segments[b][0].x || y != segments[b][0].y) && (x != segments[b][1].x || y != segments[b][1].y)) {
					// splitting b
					drawvec dv;
					dv.push_back(draw(VT_MOVETO, x, y));
					dv.push_back(segments[b][1]);
					segments.push_back(dv);
					segments[b][1] = draw(VT_LINETO, x, y);
				}
			}
		}
	}
}

std::vector<drawvec> intersect_segments(std::vector<drawvec> segments) {
	bool again = true;

	printf("0 setlinewidth .5 .5 1 setrgbcolor\n");
	for (size_t i = 0; i < segments.size(); i++) {
		printf("%lld %lld moveto %lld %lld lineto stroke\n",
		       segments[i][0].x, 4095 - segments[i][0].y,
		       segments[i][1].x, 4095 - segments[i][1].y);
	}

	while (again) {
		again = false;

		std::multimap<long long, size_t> starts;
		std::multimap<long long, size_t> ends;
		std::vector<long long> transitions;
		std::set<size_t> active;

		std::set<std::pair<size_t, size_t>> possible;

		for (size_t i = 0; i < segments.size(); i++) {
			long long top, bottom;

			if (segments[i][0].y < segments[i][1].y) {
				top = segments[i][0].y;
				bottom = segments[i][1].y;
			} else {
				top = segments[i][1].y;
				bottom = segments[i][0].y;
			}

			starts.insert(std::pair<const long long, size_t>(top, i));
			ends.insert(std::pair<const long long, size_t>(bottom, i));

			transitions.push_back(top);
			transitions.push_back(bottom);
		}

		std::sort(transitions.begin(), transitions.end());

		for (size_t i = 0; i < transitions.size(); i++) {
			while (i + 1 < transitions.size() && transitions[i + 1] == transitions[i]) {
				i++;
			}

			auto st = starts.equal_range(transitions[i]);
			for (std::multimap<long long, size_t>::iterator it = st.first; it != st.second; ++it) {
				active.insert(it->second);
			}

			std::multimap<long long, size_t> h_starts;
			std::multimap<long long, size_t> h_ends;
			std::vector<long long> h_transitions;
			std::set<size_t> h_active;

			for (std::set<size_t>::iterator it = active.begin(); it != active.end(); ++it) {
				long long left, right;

				if (segments[*it][0].x < segments[*it][1].x) {
					left = segments[*it][0].x;
					right = segments[*it][1].x;
				} else {
					left = segments[*it][1].x;
					right = segments[*it][0].x;
				}

				h_starts.insert(std::pair<const long long, size_t>(left, *it));
				h_ends.insert(std::pair<const long long, size_t>(right, *it));

				h_transitions.push_back(left);
				h_transitions.push_back(right);
			}

			std::sort(h_transitions.begin(), h_transitions.end());

			for (size_t j = 0; j < h_transitions.size(); j++) {
				while (j + 1 < h_transitions.size() && h_transitions[j + 1] == h_transitions[j]) {
					j++;
				}

				auto hst = h_starts.equal_range(h_transitions[j]);
				for (std::multimap<long long, size_t>::iterator hit = hst.first; hit != hst.second; ++hit) {
					h_active.insert(hit->second);
				}

				if (h_active.size() > 1) {
					std::vector<size_t> tocheck;

					for (std::set<size_t>::iterator hit = h_active.begin(); hit != h_active.end(); ++hit) {
						tocheck.push_back(*hit);
					}

					for (size_t ii = 0; ii < tocheck.size(); ii++) {
						for (size_t jj = ii + 1; jj < tocheck.size(); jj++) {
							if (tocheck[ii] < tocheck[jj]) {
								possible.insert(std::pair<size_t, size_t>(tocheck[ii], tocheck[jj]));
							}
						}
					}
				}

				auto hen = h_ends.equal_range(h_transitions[j]);
				for (std::multimap<long long, size_t>::iterator hit = hen.first; hit != hen.second; ++hit) {
					h_active.erase(hit->second);
				}
			}

			auto en = ends.equal_range(transitions[i]);
			for (std::multimap<long long, size_t>::iterator it = en.first; it != en.second; ++it) {
				active.erase(it->second);
			}
		}

		fprintf(stderr, "checking %lu with %lu\n", possible.size(), segments.size());
		for (auto it = possible.begin(); it != possible.end(); ++it) {
			check_intersection(segments, it->first, it->second, again);
		}

		if (again) {
			fprintf(stderr, "again!\n");
		}
	}

#if 0
	printf("0 setlinewidth 0 setgray\n");
	for (size_t i = 0; i < segments.size(); i++) {
		printf("%lld %lld moveto %lld %lld lineto stroke\n",
		       segments[i][0].x, 4095 - segments[i][0].y,
		       segments[i][1].x, 4095 - segments[i][1].y);
		printf("%lld %lld .25 0 360 arc fill %lld %lld .25 0 360 arc fill\n",
		       segments[i][0].x, 4095 - segments[i][0].y,
		       segments[i][1].x, 4095 - segments[i][1].y);
	}
#endif

	return segments;
}

drawvec clean_polygon(drawvec &geom) {
	std::vector<drawvec> segments;

	// Note that this assumes that polygons are closed.
	// If they do not duplicate the last point, the last
	// segment will need to be added explicitly.

	for (size_t i = 1; i < geom.size(); i++) {
		if (geom[i].op == VT_LINETO) {
			if (geom[i].x != geom[i - 1].x || geom[i].y != geom[i - 1].y) {
				drawvec dv;
				dv.push_back(geom[i - 1]);
				dv.push_back(geom[i]);
				segments.push_back(dv);
			}
		}
	}

	segments = intersect_segments(segments);

	// Sort for stable order between runs
	std::sort(segments.begin(), segments.end());

	std::multimap<draw, size_t> paths;
	for (size_t i = 0; i < segments.size(); i++) {
		paths.insert(std::pair<draw, size_t>(segments[i][0], i));
		paths.insert(std::pair<draw, size_t>(segments[i][1], i));
	}

	std::vector<drawvec> rings;

	for (size_t i = 0; i < segments.size(); i++) {
		// fprintf(stderr, "doing segment %lu\n", i);

		if (segments[i].size() > 0) {
			drawvec ring;

			ring.push_back(segments[i][0]);
			ring.push_back(segments[i][1]);
			segments[i].clear();

			while (ring.size() > 1 && ring[0] != ring[ring.size() - 1]) {
				auto match = paths.equal_range(ring[ring.size() - 1]);

				bool unspiked = false;
				// Look for spikes
				for (auto mi = match.first; mi != match.second; ++mi) {
					size_t m = mi->second;
					// fprintf(stderr, "looking at match %lu\n", m);

					if (segments[m].size() > 0 &&
					    segments[m][0] == ring[ring.size() - 1] &&
					    segments[m][1] == ring[ring.size() - 2]) {
						fprintf(stderr, "%lu is a spike\n", m);
						segments[m].clear();
						ring.resize(ring.size() - 1);
						unspiked = true;
						break;
					}
				}

				// fprintf(stderr, "done looking at spikes\n");

				if (unspiked) {
					// fprintf(stderr, "looping with size %lu\n", ring.size());
					continue;
				}

				draw prev = ring[ring.size() - 2];
				draw here = ring[ring.size() - 1];
				double found_something = false;
				std::multimap<double, size_t> exits;

				for (auto mi = match.first; mi != match.second; ++mi) {
					size_t m = mi->second;

					double ang = atan2(prev.y - here.y, prev.x - here.x);

					if (segments[m].size() > 0) {
						draw next;

						if (segments[m][0] == here) {
							next = segments[m][1];
						} else {
							next = segments[m][0];
						}

						double ang2 = ang - atan2(next.y - here.y, next.x - here.x);
						if (ang2 < 0) {
							ang2 += 2 * M_PI;
						}

						exits.insert(std::pair<double, size_t>(ang2, m));
					}
				}

				int depth = 0;

				for (auto ei = exits.begin(); ei != exits.end(); ++ei) {
#if 0
                    printf("exit from %lld,%lld to %lld,%lld: %lf is %lld,%lld to %lld,%lld\n",
                           prev.x, prev.y, here.x, here.y,
                           ei->first,
                           segments[ei->second][0].x, segments[ei->second][0].y,
                           segments[ei->second][1].x, segments[ei->second][1].y);
#endif

					if (segments[ei->second][1] == here) {
						// Points inward
						depth++;
					} else {
						depth--;
						if (depth < 0) {
							ring.push_back(segments[ei->second][1]);
							segments[ei->second].clear();
							found_something = true;
							break;
						}
					}
				}

				if (!found_something) {
					fprintf(stderr, "couldn't find a way out\n");
					exit(EXIT_FAILURE);
					break;
				}
			}

			// fprintf(stderr, "out of ring loop\n");

			if (ring.size() > 1) {
				printf(".5 .setopacityalpha 0 setlinewidth 0 setgray newpath\n");
				for (size_t j = 0; j < ring.size(); j++) {
					printf("%lld %lld %s\n", ring[j].x, 4095 - ring[j].y,
					       j == 0 ? "moveto" : "lineto");
				}
				printf("closepath fill\n");

				rings.push_back(ring);
			}
		}
	}

	return drawvec();
}

drawvec clean_or_clip_poly(drawvec &geom, int z, int detail, int buffer, bool clip) {
	return clean_polygon(geom);

	ClipperLib::Clipper clipper(ClipperLib::ioStrictlySimple);

	bool has_area = false;

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			size_t j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op != VT_LINETO) {
					break;
				}
			}

			double area = get_area(geom, i, j);
			if (area != 0) {
				has_area = true;
			}

			ClipperLib::Path path;

			drawvec tmp;
			for (size_t k = i; k < j; k++) {
				path.push_back(ClipperLib::IntPoint(geom[k].x, geom[k].y));
			}

			if (!clipper.AddPath(path, ClipperLib::ptSubject, true)) {
#if 0
				fprintf(stderr, "Couldn't add polygon for clipping:");
				for (size_t k = i; k < j; k++) {
					fprintf(stderr, " %lld,%lld", geom[k].x, geom[k].y);
				}
				fprintf(stderr, "\n");
#endif
			}

			i = j - 1;
		} else {
			fprintf(stderr, "Unexpected operation in polygon %d\n", (int) geom[i].op);
			exit(EXIT_FAILURE);
		}
	}

	if (clip) {
		long long area = 1LL << (32 - z);
		long long clip_buffer = buffer * area / 256;

		ClipperLib::Path edge;
		edge.push_back(ClipperLib::IntPoint(-clip_buffer, -clip_buffer));
		edge.push_back(ClipperLib::IntPoint(area + clip_buffer, -clip_buffer));
		edge.push_back(ClipperLib::IntPoint(area + clip_buffer, area + clip_buffer));
		edge.push_back(ClipperLib::IntPoint(-clip_buffer, area + clip_buffer));
		edge.push_back(ClipperLib::IntPoint(-clip_buffer, -clip_buffer));

		clipper.AddPath(edge, ClipperLib::ptClip, true);
	}

	ClipperLib::PolyTree clipped;
	if (clip) {
		if (!clipper.Execute(ClipperLib::ctIntersection, clipped)) {
			fprintf(stderr, "Polygon clip failed\n");
		}
	} else {
		if (!has_area) {
			drawvec out;
			return out;
		}

		if (!clipper.Execute(ClipperLib::ctUnion, clipped)) {
			static bool complained = false;

			if (!complained) {
				fprintf(stderr, "Polygon clean failed\n");
				complained = true;
			}
		}
	}

	drawvec out;

	for (int i = 0; i < clipped.ChildCount(); i++) {
		decode_clipped(clipped.Childs[i], out);
	}

	return out;
}

/* pnpoly:
Copyright (c) 1970-2003, Wm. Randolph Franklin

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimers.
Redistributions in binary form must reproduce the above copyright notice in the documentation and/or other materials provided with the distribution.
The name of W. Randolph Franklin may not be used to endorse or promote products derived from this Software without specific prior written permission.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

static int pnpoly(drawvec &vert, size_t start, size_t nvert, long long testx, long long testy) {
	size_t i, j;
	bool c = false;
	for (i = 0, j = nvert - 1; i < nvert; j = i++) {
		if (((vert[i + start].y > testy) != (vert[j + start].y > testy)) &&
		    (testx < (vert[j + start].x - vert[i + start].x) * (testy - vert[i + start].y) / (double) (vert[j + start].y - vert[i + start].y) + vert[i + start].x))
			c = !c;
	}
	return c;
}

void check_polygon(drawvec &geom, drawvec &before) {
	for (size_t i = 0; i + 1 < geom.size(); i++) {
		for (size_t j = i + 1; j + 1 < geom.size(); j++) {
			if (geom[i + 1].op == VT_LINETO && geom[j + 1].op == VT_LINETO) {
				double s1_x = geom[i + 1].x - geom[i + 0].x;
				double s1_y = geom[i + 1].y - geom[i + 0].y;
				double s2_x = geom[j + 1].x - geom[j + 0].x;
				double s2_y = geom[j + 1].y - geom[j + 0].y;

				double s, t;
				s = (-s1_y * (geom[i + 0].x - geom[j + 0].x) + s1_x * (geom[i + 0].y - geom[j + 0].y)) / (-s2_x * s1_y + s1_x * s2_y);
				t = (s2_x * (geom[i + 0].y - geom[j + 0].y) - s2_y * (geom[i + 0].x - geom[j + 0].x)) / (-s2_x * s1_y + s1_x * s2_y);

				if (t > 0 && t < 1 && s > 0 && s < 1) {
					printf("Internal error: self-intersecting polygon. %lld,%lld to %lld,%lld intersects %lld,%lld to %lld,%lld\n",
					       geom[i + 0].x, geom[i + 0].y,
					       geom[i + 1].x, geom[i + 1].y,
					       geom[j + 0].x, geom[j + 0].y,
					       geom[j + 1].x, geom[j + 1].y);
					dump(before);
				}
			}
		}
	}

	size_t outer_start = -1;
	size_t outer_len = 0;

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			size_t j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op != VT_LINETO) {
					break;
				}
			}

			double area = get_area(geom, i, j);

#if 0
			fprintf(stderr, "looking at %lld to %lld, area %f\n", (long long) i, (long long) j, area);
#endif

			if (area > 0) {
				outer_start = i;
				outer_len = j - i;
			} else {
				for (size_t k = i; k < j; k++) {
					if (!pnpoly(geom, outer_start, outer_len, geom[k].x, geom[k].y)) {
						bool on_edge = false;

						for (size_t l = outer_start; l < outer_start + outer_len; l++) {
							if (geom[k].x == geom[l].x || geom[k].y == geom[l].y) {
								on_edge = true;
								break;
							}
						}

						if (!on_edge) {
							printf("%lld,%lld at %lld not in outer ring (%lld to %lld)\n", geom[k].x, geom[k].y, (long long) k, (long long) outer_start, (long long) (outer_start + outer_len));

							dump(before);
#if 0
							for (size_t l = outer_start; l < outer_start + outer_len; l++) {
								fprintf(stderr, " %lld,%lld", geom[l].x, geom[l].y);
							}
#endif
						}
					}
				}
			}
		}
	}
}

drawvec close_poly(drawvec &geom) {
	drawvec out;

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			size_t j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op != VT_LINETO) {
					break;
				}
			}

			if (j - 1 > i) {
				if (geom[j - 1].x != geom[i].x || geom[j - 1].y != geom[i].y) {
					fprintf(stderr, "Internal error: polygon not closed\n");
				}
			}

			for (size_t n = i; n < j - 1; n++) {
				out.push_back(geom[n]);
			}
			out.push_back(draw(VT_CLOSEPATH, 0, 0));

			i = j - 1;
		}
	}

	return out;
}

static bool inside(draw d, int edge, long long minx, long long miny, long long maxx, long long maxy) {
	switch (edge) {
	case 0:  // top
		return d.y > miny;

	case 1:  // right
		return d.x < maxx;

	case 2:  // bottom
		return d.y < maxy;

	case 3:  // left
		return d.x > minx;
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
	// s = (-s1_y * (p0.x - p2.x) + s1_x * (p0.y - p2.y)) / (-s2_x * s1_y + s1_x * s2_y);
	t = (s2_x * (p0.y - p2.y) - s2_y * (p0.x - p2.x)) / (-s2_x * s1_y + s1_x * s2_y);

	return draw(VT_LINETO, p0.x + (t * s1_x), p0.y + (t * s1_y));
}

static draw intersect(draw a, draw b, int edge, long long minx, long long miny, long long maxx, long long maxy) {
	switch (edge) {
	case 0:  // top
		return get_line_intersection(a, b, draw(VT_MOVETO, minx, miny), draw(VT_MOVETO, maxx, miny));
		break;

	case 1:  // right
		return get_line_intersection(a, b, draw(VT_MOVETO, maxx, miny), draw(VT_MOVETO, maxx, maxy));
		break;

	case 2:  // bottom
		return get_line_intersection(a, b, draw(VT_MOVETO, maxx, maxy), draw(VT_MOVETO, minx, maxy));
		break;

	case 3:  // left
		return get_line_intersection(a, b, draw(VT_MOVETO, minx, maxy), draw(VT_MOVETO, minx, miny));
		break;
	}

	fprintf(stderr, "internal error intersecting\n");
	exit(EXIT_FAILURE);
}

// http://en.wikipedia.org/wiki/Sutherland%E2%80%93Hodgman_algorithm
static drawvec clip_poly1(drawvec &geom, long long minx, long long miny, long long maxx, long long maxy) {
	drawvec out = geom;

	for (int edge = 0; edge < 4; edge++) {
		if (out.size() > 0) {
			drawvec in = out;
			out.resize(0);

			draw S = in[in.size() - 1];

			for (size_t e = 0; e < in.size(); e++) {
				draw E = in[e];

				if (inside(E, edge, minx, miny, maxx, maxy)) {
					if (!inside(S, edge, minx, miny, maxx, maxy)) {
						out.push_back(intersect(S, E, edge, minx, miny, maxx, maxy));
					}
					out.push_back(E);
				} else if (inside(S, edge, minx, miny, maxx, maxy)) {
					out.push_back(intersect(S, E, edge, minx, miny, maxx, maxy));
				}

				S = E;
			}
		}
	}

	if (out.size() > 0) {
		// If the polygon begins and ends outside the edge,
		// the starting and ending points will be left as the
		// places where it intersects the edge. Need to add
		// another point to close the loop.

		if (out[0].x != out[out.size() - 1].x || out[0].y != out[out.size() - 1].y) {
			out.push_back(out[0]);
		}

		if (out.size() < 3) {
			// fprintf(stderr, "Polygon degenerated to a line segment\n");
			out.clear();
			return out;
		}

		out[0].op = VT_MOVETO;
		for (size_t i = 1; i < out.size(); i++) {
			out[i].op = VT_LINETO;
		}
	}

	return out;
}

drawvec simple_clip_poly(drawvec &geom, long long minx, long long miny, long long maxx, long long maxy) {
	drawvec out;

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			size_t j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op != VT_LINETO) {
					break;
				}
			}

			drawvec tmp;
			for (size_t k = i; k < j; k++) {
				tmp.push_back(geom[k]);
			}
			tmp = clip_poly1(tmp, minx, miny, maxx, maxy);
			if (tmp.size() > 0) {
				if (tmp[0].x != tmp[tmp.size() - 1].x || tmp[0].y != tmp[tmp.size() - 1].y) {
					fprintf(stderr, "Internal error: Polygon ring not closed\n");
					exit(EXIT_FAILURE);
				}
			}
			for (size_t k = 0; k < tmp.size(); k++) {
				out.push_back(tmp[k]);
			}

			i = j - 1;
		} else {
			fprintf(stderr, "Unexpected operation in polygon %d\n", (int) geom[i].op);
			exit(EXIT_FAILURE);
		}
	}

	return out;
}

drawvec simple_clip_poly(drawvec &geom, int z, int detail, int buffer) {
	long long area = 1LL << (32 - z);
	long long clip_buffer = buffer * area / 256;

	return simple_clip_poly(geom, -clip_buffer, -clip_buffer, area + clip_buffer, area + clip_buffer);
}

drawvec reduce_tiny_poly(drawvec &geom, int z, int detail, bool *reduced, double *accum_area) {
	drawvec out;
	long long pixel = (1 << (32 - detail - z)) * 2;

	*reduced = true;
	bool included_last_outer = false;

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			size_t j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op != VT_LINETO) {
					break;
				}
			}

			double area = get_area(geom, i, j);

			// XXX There is an ambiguity here: If the area of a ring is 0 and it is followed by holes,
			// we don't know whether the area-0 ring was a hole too or whether it was the outer ring
			// that these subsequent holes are somehow being subtracted from. I hope that if a polygon
			// was simplified down to nothing, its holes also became nothing.

			if (area != 0) {
				// These are pixel coordinates, so area > 0 for the outer ring.
				// If the outer ring of a polygon was reduced to a pixel, its
				// inner rings must just have their area de-accumulated rather
				// than being drawn since we don't really know where they are.

				if (std::fabs(area) <= pixel * pixel || (area < 0 && !included_last_outer)) {
					// printf("area is only %f vs %lld so using square\n", area, pixel * pixel);

					*accum_area += area;
					if (area > 0 && *accum_area > pixel * pixel) {
						// XXX use centroid;

						out.push_back(draw(VT_MOVETO, geom[i].x - pixel / 2, geom[i].y - pixel / 2));
						out.push_back(draw(VT_LINETO, geom[i].x + pixel / 2, geom[i].y - pixel / 2));
						out.push_back(draw(VT_LINETO, geom[i].x + pixel / 2, geom[i].y + pixel / 2));
						out.push_back(draw(VT_LINETO, geom[i].x - pixel / 2, geom[i].y + pixel / 2));
						out.push_back(draw(VT_LINETO, geom[i].x - pixel / 2, geom[i].y - pixel / 2));

						*accum_area -= pixel * pixel;
					}

					if (area > 0) {
						included_last_outer = false;
					}
				} else {
					// printf("area is %f so keeping instead of %lld\n", area, pixel * pixel);

					for (size_t k = i; k <= j && k < geom.size(); k++) {
						out.push_back(geom[k]);
					}

					*reduced = false;

					if (area > 0) {
						included_last_outer = true;
					}
				}
			}

			i = j - 1;
		} else {
			fprintf(stderr, "how did we get here with %d in %d?\n", geom[i].op, (int) geom.size());

			for (size_t n = 0; n < geom.size(); n++) {
				fprintf(stderr, "%d/%lld/%lld ", geom[n].op, geom[n].x, geom[n].y);
			}
			fprintf(stderr, "\n");

			out.push_back(geom[i]);
		}
	}

	return out;
}

drawvec clip_point(drawvec &geom, int z, int detail, long long buffer) {
	drawvec out;

	long long min = 0;
	long long area = 1LL << (32 - z);

	min -= buffer * area / 256;
	area += buffer * area / 256;

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].x >= min && geom[i].y >= min && geom[i].x <= area && geom[i].y <= area) {
			out.push_back(geom[i]);
		}
	}

	return out;
}

int quick_check(long long *bbox, int z, int detail, long long buffer) {
	long long min = 0;
	long long area = 1LL << (32 - z);

	min -= buffer * area / 256;
	area += buffer * area / 256;

	// bbox entirely outside the tile
	if (bbox[0] > area || bbox[1] > area) {
		return 0;
	}
	if (bbox[2] < min || bbox[3] < min) {
		return 0;
	}

	// bbox entirely within the tile
	if (bbox[0] > min && bbox[1] > min && bbox[2] < area && bbox[3] < area) {
		return 1;
	}

	// some overlap of edge
	return 2;
}

bool point_within_tile(long long x, long long y, int z, int detail, long long buffer) {
	// No adjustment for buffer, because the point must be
	// strictly within the tile to appear exactly once

	long long area = 1LL << (32 - z);

	return x >= 0 && y >= 0 && x < area && y < area;
}

drawvec clip_lines(drawvec &geom, int z, int detail, long long buffer) {
	drawvec out;

	long long min = 0;
	long long area = 1LL << (32 - z);
	min -= buffer * area / 256;
	area += buffer * area / 256;

	for (size_t i = 0; i < geom.size(); i++) {
		if (i > 0 && (geom[i - 1].op == VT_MOVETO || geom[i - 1].op == VT_LINETO) && geom[i].op == VT_LINETO) {
			double x1 = geom[i - 1].x;
			double y1 = geom[i - 1].y;

			double x2 = geom[i - 0].x;
			double y2 = geom[i - 0].y;

			int c = clip(&x1, &y1, &x2, &y2, min, min, area, area);

			if (c > 1) {  // clipped
				out.push_back(draw(VT_MOVETO, x1, y1));
				out.push_back(draw(VT_LINETO, x2, y2));
				out.push_back(draw(VT_MOVETO, geom[i].x, geom[i].y));
			} else if (c == 1) {  // unchanged
				out.push_back(geom[i]);
			} else {  // clipped away entirely
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
			double temp_dist = square_distance_from_line(geom[start + i].x, geom[start + i].y, geom[start + first].x, geom[start + first].y, geom[start + second].x, geom[start + second].y);

			double distance = std::fabs(temp_dist);

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

// If any line segment crosses a tile boundary, add a node there
// that cannot be simplified away, to prevent the edge of any
// feature from jumping abruptly at the tile boundary.
drawvec impose_tile_boundaries(drawvec &geom, long long extent) {
	drawvec out;

	for (size_t i = 0; i < geom.size(); i++) {
		if (i > 0 && geom[i].op == VT_LINETO && (geom[i - 1].op == VT_MOVETO || geom[i - 1].op == VT_LINETO)) {
			double x1 = geom[i - 1].x;
			double y1 = geom[i - 1].y;

			double x2 = geom[i - 0].x;
			double y2 = geom[i - 0].y;

			int c = clip(&x1, &y1, &x2, &y2, 0, 0, extent, extent);

			if (c > 1) {  // clipped
				if (x1 != geom[i - 1].x || y1 != geom[i - 1].y) {
					out.push_back(draw(VT_LINETO, x1, y1));
					out[out.size() - 1].necessary = 1;
				}
				if (x2 != geom[i - 0].x || y2 != geom[i - 0].y) {
					out.push_back(draw(VT_LINETO, x2, y2));
					out[out.size() - 1].necessary = 1;
				}
			}
		}

		out.push_back(geom[i]);
	}

	return out;
}

drawvec simplify_lines(drawvec &geom, int z, int detail, bool mark_tile_bounds) {
	int res = 1 << (32 - detail - z);
	long long area = 1LL << (32 - z);

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			geom[i].necessary = 1;
		} else if (geom[i].op == VT_LINETO) {
			geom[i].necessary = 0;
		} else {
			geom[i].necessary = 1;
		}
	}

	if (mark_tile_bounds) {
		geom = impose_tile_boundaries(geom, area);
	}

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			size_t j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op != VT_LINETO) {
					break;
				}
			}

			geom[i].necessary = 1;
			geom[j - 1].necessary = 1;

			if (j - i > 1) {
				douglas_peucker(geom, i, j - i, res);
			}
			i = j - 1;
		}
	}

	drawvec out;
	for (size_t i = 0; i < geom.size(); i++) {
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

	for (size_t i = 0; i < geom.size(); i++) {
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
		for (size_t i = 0; i < geom.size(); i++) {
			out.push_back(geom[geom.size() - 1 - i]);
		}
		out[0].op = VT_MOVETO;
		out[out.size() - 1].op = VT_LINETO;
		return out;
	}

	return geom;
}

drawvec fix_polygon(drawvec &geom) {
	int outer = 1;
	drawvec out;

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_CLOSEPATH) {
			outer = 1;
		} else if (geom[i].op == VT_MOVETO) {
			// Find the end of the ring

			size_t j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op != VT_LINETO) {
					break;
				}
			}

			// Make a temporary copy of the ring.
			// Close it if it isn't closed.

			drawvec ring;
			for (size_t a = i; a < j; a++) {
				ring.push_back(geom[a]);
			}
			if (j - i != 0 && (ring[0].x != ring[j - i - 1].x || ring[0].y != ring[j - i - 1].y)) {
				ring.push_back(ring[0]);
			}

			// Reverse ring if winding order doesn't match
			// inner/outer expectation

			double area = get_area(ring, 0, ring.size());
			if ((area > 0) != outer) {
				drawvec tmp;
				for (int a = ring.size() - 1; a >= 0; a--) {
					tmp.push_back(ring[a]);
				}
				ring = tmp;
			}

			// Copy ring into output, fixing the moveto/lineto ops if necessary because of
			// reversal or closing

			for (size_t a = 0; a < ring.size(); a++) {
				if (a == 0) {
					out.push_back(draw(VT_MOVETO, ring[a].x, ring[a].y));
				} else {
					out.push_back(draw(VT_LINETO, ring[a].x, ring[a].y));
				}
			}

			// Next ring or polygon begins on the non-lineto that ended this one
			// and is not an outer ring unless there is a terminator first

			i = j - 1;
			outer = 0;
		} else {
			fprintf(stderr, "Internal error: polygon ring begins with %d, not moveto\n", geom[i].op);
			exit(EXIT_FAILURE);
		}
	}

	return out;
}

std::vector<drawvec> chop_polygon(std::vector<drawvec> &geoms) {
	while (1) {
		bool again = false;
		std::vector<drawvec> out;

		for (size_t i = 0; i < geoms.size(); i++) {
			if (geoms[i].size() > 700) {
				static bool warned = false;
				if (!warned) {
					fprintf(stderr, "Warning: splitting up polygon with more than 700 sides\n");
					warned = true;
				}

				long long midx = 0, midy = 0, count = 0;
				long long maxx = LLONG_MIN, maxy = LLONG_MIN, minx = LLONG_MAX, miny = LLONG_MAX;

				for (size_t j = 0; j < geoms[i].size(); j++) {
					if (geoms[i][j].op == VT_MOVETO || geoms[i][j].op == VT_LINETO) {
						midx += geoms[i][j].x;
						midy += geoms[i][j].y;
						count++;

						if (geoms[i][j].x > maxx) {
							maxx = geoms[i][j].x;
						}
						if (geoms[i][j].y > maxy) {
							maxy = geoms[i][j].y;
						}
						if (geoms[i][j].x < minx) {
							minx = geoms[i][j].x;
						}
						if (geoms[i][j].y < miny) {
							miny = geoms[i][j].y;
						}
					}
				}

				midx /= count;
				midy /= count;

				drawvec c1, c2;

				if (maxy - miny > maxx - minx) {
					// printf("clipping y to %lld %lld %lld %lld\n", minx, miny, maxx, midy);
					c1 = simple_clip_poly(geoms[i], minx, miny, maxx, midy);
					// printf("          and %lld %lld %lld %lld\n", minx, midy, maxx, maxy);
					c2 = simple_clip_poly(geoms[i], minx, midy, maxx, maxy);
				} else {
					// printf("clipping x to %lld %lld %lld %lld\n", minx, miny, midx, maxy);
					c1 = simple_clip_poly(geoms[i], minx, miny, midx, maxy);
					// printf("          and %lld %lld %lld %lld\n", midx, midy, maxx, maxy);
					c2 = simple_clip_poly(geoms[i], midx, miny, maxx, maxy);
				}

				if (c1.size() >= geoms[i].size()) {
					fprintf(stderr, "Subdividing complex polygon failed\n");
				} else {
					out.push_back(c1);
				}
				if (c2.size() >= geoms[i].size()) {
					fprintf(stderr, "Subdividing complex polygon failed\n");
				} else {
					out.push_back(c2);
				}

				again = true;
			} else {
				out.push_back(geoms[i]);
			}
		}

		if (!again) {
			return out;
		}

		geoms = out;
	}
}

#define INSIDE 0
#define LEFT 1
#define RIGHT 2
#define BOTTOM 4
#define TOP 8

static int computeOutCode(double x, double y, double xmin, double ymin, double xmax, double ymax) {
	int code = INSIDE;

	if (x < xmin) {
		code |= LEFT;
	} else if (x > xmax) {
		code |= RIGHT;
	}

	if (y < ymin) {
		code |= BOTTOM;
	} else if (y > ymax) {
		code |= TOP;
	}

	return code;
}

static int clip(double *x0, double *y0, double *x1, double *y1, double xmin, double ymin, double xmax, double ymax) {
	int outcode0 = computeOutCode(*x0, *y0, xmin, ymin, xmax, ymax);
	int outcode1 = computeOutCode(*x1, *y1, xmin, ymin, xmax, ymax);
	int accept = 0;
	int changed = 0;

	while (1) {
		if (!(outcode0 | outcode1)) {  // Bitwise OR is 0. Trivially accept and get out of loop
			accept = 1;
			break;
		} else if (outcode0 & outcode1) {  // Bitwise AND is not 0. Trivially reject and get out of loop
			break;
		} else {
			// failed both tests, so calculate the line segment to clip
			// from an outside point to an intersection with clip edge
			double x = *x0, y = *y0;

			// At least one endpoint is outside the clip rectangle; pick it.
			int outcodeOut = outcode0 ? outcode0 : outcode1;

			// Now find the intersection point;
			// use formulas y = y0 + slope * (x - x0), x = x0 + (1 / slope) * (y - y0)
			if (outcodeOut & TOP) {  // point is above the clip rectangle
				x = *x0 + (*x1 - *x0) * (ymax - *y0) / (*y1 - *y0);
				y = ymax;
			} else if (outcodeOut & BOTTOM) {  // point is below the clip rectangle
				x = *x0 + (*x1 - *x0) * (ymin - *y0) / (*y1 - *y0);
				y = ymin;
			} else if (outcodeOut & RIGHT) {  // point is to the right of clip rectangle
				y = *y0 + (*y1 - *y0) * (xmax - *x0) / (*x1 - *x0);
				x = xmax;
			} else if (outcodeOut & LEFT) {  // point is to the left of clip rectangle
				y = *y0 + (*y1 - *y0) * (xmin - *x0) / (*x1 - *x0);
				x = xmin;
			}

			// Now we move outside point to intersection point to clip
			// and get ready for next pass.
			if (outcodeOut == outcode0) {
				*x0 = x;
				*y0 = y;
				outcode0 = computeOutCode(*x0, *y0, xmin, ymin, xmax, ymax);
				changed = 1;
			} else {
				*x1 = x;
				*y1 = y;
				outcode1 = computeOutCode(*x1, *y1, xmin, ymin, xmax, ymax);
				changed = 1;
			}
		}
	}

	if (accept == 0) {
		return 0;
	} else {
		return changed + 1;
	}
}
