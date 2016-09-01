#include <iostream>
#include <fstream>
#include <string>
#include <stack>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <unistd.h>
#include <cmath>
#include <limits.h>
#include <sqlite3.h>
#include "geometry.hpp"
#include "polygon.hpp"

struct ring {
	drawvec data;
	long double area;
	long long parent;
	std::vector<size_t> children;
	int depth;

	ring(drawvec &_data) {
		data = _data;
		area = get_area(_data, 0, _data.size());
		parent = -1;
		depth = -999;
	}

	bool operator<(const ring &o) const {
		if (std::fabs(this->area) < std::fabs(o.area)) {
			return true;
		} else if (std::fabs(this->area) == std::fabs(o.area)) {
			return this->data < o.data;
		} else {
			return false;
		}
	}
};

/* pnpoly:
Copyright (c) 1970-2003, Wm. Randolph Franklin

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimers.
Redistributions in binary form must reproduce the above copyright notice in the documentation and/or other materials provided with the distribution.
The name of W. Randolph Franklin may not be used to endorse or promote products derived from this Software without specific prior written permission.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

static int pnpoly(drawvec &vert, size_t start, size_t nvert, double testx, double testy) {
	size_t i, j;
	bool c = false;
	for (i = 0, j = nvert - 1; i < nvert; j = i++) {
		if (((vert[i + start].y > testy) != (vert[j + start].y > testy)) &&
		    (testx < (vert[j + start].x - vert[i + start].x) * (testy - vert[i + start].y) / (double) (vert[j + start].y - vert[i + start].y) + vert[i + start].x))
			c = !c;
	}
	return c;
}

void add_vertical(size_t intermediate, size_t which_end, size_t into, std::vector<drawvec> &segments, bool &again) {
	again = true;
	drawvec dv;
	dv.push_back(segments[intermediate][which_end]);
	dv.push_back(segments[into][1]);
	dv[0].op = dv[1].op;
	segments.push_back(dv);
	segments[into][1] = segments[intermediate][which_end];
	segments[into][1].op = segments[into][0].op;
}

void add_horizontal(size_t intermediate, size_t which_end, size_t into, std::vector<drawvec> &segments, bool &again) {
	again = true;

	long long x = segments[intermediate][which_end].x;
	long long y = segments[intermediate][0].y +
		      (segments[intermediate][which_end].x - segments[intermediate][0].x) *
			      (segments[intermediate][1].y - segments[intermediate][0].y) /
			      (segments[intermediate][1].x - segments[intermediate][0].x);
	draw d(segments[into][0].op, x, y);

	drawvec dv;
	dv.push_back(d);
	dv.push_back(segments[into][1]);
	segments.push_back(dv);
	segments[into][1] = d;
}

void check_intersection(std::vector<drawvec> &segments, size_t a, size_t b, bool &again) {
	long long s10_x = segments[a][1].x - segments[a][0].x;
	long long s10_y = segments[a][1].y - segments[a][0].y;
	long long s32_x = segments[b][1].x - segments[b][0].x;
	long long s32_y = segments[b][1].y - segments[b][0].y;

	// http://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
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
			if (segments[a][0].x == segments[a][1].x) {
				// Vertical

				long long amin, amax, bmin, bmax;
				if (segments[a][0].y < segments[a][1].y) {
					amin = segments[a][0].y;
					amax = segments[a][1].y;
				} else {
					amin = segments[a][1].y;
					amax = segments[a][0].y;
				}
				if (segments[b][0].y < segments[b][1].y) {
					bmin = segments[b][0].y;
					bmax = segments[b][1].y;
				} else {
					bmin = segments[b][1].y;
					bmax = segments[b][0].y;
				}

				// All of these transformations preserve verticality so we can check multiple cases
				if (segments[b][0].y > amin && segments[b][0].y < amax) {
					// B0 is in A
					add_vertical(b, 0, a, segments, again);
				}
				if (segments[b][1].y > amin && segments[b][1].y < amax) {
					// B1 is in A
					add_vertical(b, 1, a, segments, again);
				}
				if (segments[a][0].y > bmin && segments[a][0].y < bmax) {
					// A0 is in B
					add_vertical(a, 0, b, segments, again);
				}
				if (segments[a][1].y > bmin && segments[a][1].y < bmax) {
					// A1 is in B
					add_vertical(a, 1, b, segments, again);
				}
			} else {
				// Horizontal or diagonal

				long long amin, amax, bmin, bmax;
				if (segments[a][0].x < segments[a][1].x) {
					amin = segments[a][0].x;
					amax = segments[a][1].x;
				} else {
					amin = segments[a][1].x;
					amax = segments[a][0].x;
				}
				if (segments[b][0].x < segments[b][1].x) {
					bmin = segments[b][0].x;
					bmax = segments[b][1].x;
				} else {
					bmin = segments[b][1].x;
					bmax = segments[b][0].x;
				}

				// Don't check multiples, because rounding may corrupt collinearity
				if (segments[b][0].x > amin && segments[b][0].x < amax) {
					// B0 is in A
					add_horizontal(b, 0, a, segments, again);
				} else if (segments[b][1].x > amin && segments[b][1].x < amax) {
					// B1 is in A
					add_horizontal(b, 1, a, segments, again);
				} else if (segments[a][0].x > bmin && segments[a][0].x < bmax) {
					// A0 is in B
					add_horizontal(a, 0, b, segments, again);
				} else if (segments[a][1].x > bmin && segments[a][1].x < bmax) {
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

		if (t >= 0 && t <= 1 && s >= 0 && s <= 1) {
			long long x = round(segments[a][0].x + t * s10_x);
			long long y = round(segments[a][0].y + t * s10_y);

			if (t >= 0 && t <= 1) {
				if ((x != segments[a][0].x || y != segments[a][0].y) && (x != segments[a][1].x || y != segments[a][1].y)) {
					// splitting a
					drawvec dv;
					dv.push_back(draw(segments[a][1].op, x, y));
					dv.push_back(segments[a][1]);
					segments.push_back(dv);
					segments[a][1] = draw(segments[a][1].op, x, y);
					again = true;
				}
			}

			if (s >= 0 && s <= 1) {
				if ((x != segments[b][0].x || y != segments[b][0].y) && (x != segments[b][1].x || y != segments[b][1].y)) {
					// splitting b
					drawvec dv;
					dv.push_back(draw(segments[b][1].op, x, y));
					dv.push_back(segments[b][1]);
					segments.push_back(dv);
					segments[b][1] = draw(segments[b][1].op, x, y);
					again = true;
				}
			}
		}
	}
}

void partition(std::vector<drawvec> &segs, std::vector<size_t> &subset, int direction, std::set<std::pair<size_t, size_t>> &possible) {
	std::vector<long long> points;

	// List of X or Y midpoints of edges, so we can find the median

	if (direction == 0) {
		for (size_t i = 0; i < subset.size(); i++) {
			points.push_back((segs[subset[i]][0].x + segs[subset[i]][1].x) / 2);
		}
	} else {
		for (size_t i = 0; i < subset.size(); i++) {
			points.push_back((segs[subset[i]][0].y + segs[subset[i]][1].y) / 2);
		}
	}

	std::sort(points.begin(), points.end());
	long long median = points[points.size() / 2];

	// Partition into sets that are above or below, or to the left or to the right of, the median.
	// Segments that cross the median appear in both.

	std::vector<size_t> one;
	std::vector<size_t> two;

	if (direction == 0) {
		for (size_t i = 0; i < subset.size(); i++) {
			if (segs[subset[i]][0].x <= median || segs[subset[i]][1].x <= median) {
				one.push_back(subset[i]);
			}
			if (segs[subset[i]][0].x >= median || segs[subset[i]][1].x >= median) {
				two.push_back(subset[i]);
			}
		}
	} else {
		for (size_t i = 0; i < subset.size(); i++) {
			if (segs[subset[i]][0].y <= median || segs[subset[i]][1].y <= median) {
				one.push_back(subset[i]);
			}
			if (segs[subset[i]][0].y >= median || segs[subset[i]][1].y >= median) {
				two.push_back(subset[i]);
			}
		}
	}

	if (one.size() >= subset.size() || two.size() >= subset.size()) {
		for (size_t i = 0; i < subset.size(); i++) {
			for (size_t j = i + 1; j < subset.size(); j++) {
				possible.insert(std::pair<size_t, size_t>(subset[i], subset[j]));
			}
		}
	} else {
		// By experiment, stopping at 10 is a little faster than either 5 or 20

		if (one.size() < 10) {
			for (size_t i = 0; i < one.size(); i++) {
				for (size_t j = i + 1; j < one.size(); j++) {
					possible.insert(std::pair<size_t, size_t>(one[i], one[j]));
				}
			}
		} else {
			partition(segs, one, !direction, possible);
		}

		if (two.size() < 10) {
			for (size_t i = 0; i < two.size(); i++) {
				for (size_t j = i + 1; j < two.size(); j++) {
					possible.insert(std::pair<size_t, size_t>(two[i], two[j]));
				}
			}
		} else {
			partition(segs, two, !direction, possible);
		}
	}
}

void sanity_check(std::vector<drawvec> const &segments) {
	for (size_t i = 0; i < segments.size(); i++) {
		if (segments[i].size() != 2) {
			fprintf(stderr, "not a segment\n");
			abort();
		}

		if (segments[i][0].op != segments[i][1].op) {
			fprintf(stderr, "mismatched winding\n");
			abort();
		}
	}
}

std::vector<drawvec> intersect_segments(std::vector<drawvec> segments) {
	bool again = true;

	while (again) {
		again = false;

		std::set<std::pair<size_t, size_t>> possible;

#if 1
		std::vector<size_t> subset;
		for (size_t i = 0; i < segments.size(); i++) {
			subset.push_back(i);
		}

		partition(segments, subset, 0, possible);
#else
		std::multimap<long long, size_t> starts;
		std::multimap<long long, size_t> ends;
		std::vector<long long> transitions;
		std::set<size_t> active;

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
#endif

		for (auto it = possible.begin(); it != possible.end(); ++it) {
			check_intersection(segments, it->first, it->second, again);
		}

		// XXX
		sanity_check(segments);
	}

	return segments;
}

void assign_depth(std::vector<ring> &rings, size_t i, int depth) {
	rings[i].depth = depth;

	for (size_t j = 0; j < rings[i].children.size(); j++) {
		if (rings[rings[i].children[j]].area > 0) {
			assign_depth(rings, rings[i].children[j], depth + 1);
		} else {
			assign_depth(rings, rings[i].children[j], depth - 1);
		}
	}
}

drawvec reassemble_rings(std::vector<drawvec> &orings, bool all_rings) {
	for (size_t i = 0; i < orings.size(); i++) {
		for (size_t j = 0; j < orings[i].size(); j++) {
			if (j == 0) {
				orings[i][j].op = VT_MOVETO;
			} else {
				orings[i][j].op = VT_LINETO;
			}
		}
	}

	std::vector<ring> rings;

	for (size_t i = 0; i < orings.size(); i++) {
		ring r(orings[i]);
		rings.push_back(r);
	}
	std::sort(rings.begin(), rings.end());

	for (size_t i = 0; i < rings.size(); i++) {
		size_t count = rings[i].data.size() - 1;
		double xx, yy;

		size_t k;
		for (k = 0; k < count; k++) {
			xx = (rings[i].data[k].x + rings[i].data[(k + 1) % count].x + rings[i].data[(k + 2) % count].x) / 3.0;
			yy = (rings[i].data[k].y + rings[i].data[(k + 1) % count].y + rings[i].data[(k + 2) % count].y) / 3.0;

			if (pnpoly(rings[i].data, 0, rings[i].data.size(), xx, yy)) {
				break;
			}
		}

		if (k >= count) {
			fprintf(stderr, "Ring with no ear within\n");
			exit(EXIT_FAILURE);
		}

		for (size_t j = i + 1; j < rings.size(); j++) {
			if (pnpoly(rings[j].data, 0, rings[j].data.size(), xx, yy)) {
				rings[i].parent = j;
				rings[j].children.push_back(i);
				break;
			}
		}
	}

	if (all_rings) {
		drawvec out;

		for (size_t ii = rings.size(); ii > 0; ii--) {
			size_t i = ii - 1;

			for (size_t j = 0; j < rings[i].data.size(); j++) {
				out.push_back(rings[i].data[j]);
			}
		}

		return out;
	}

	// Find all the outer rings with no parents, which are level 1.
	// Follow their children down to assign a winding count to each.

	for (size_t ii = rings.size(); ii > 0; ii--) {
		size_t i = ii - 1;

		if (rings[i].area > 0 && rings[i].parent < 0) {
			assign_depth(rings, i, 1);
		}
	}

	// Now output each ring with a depth of 1 that either has no parent
	// or is the child of a ring with a depth of 0, followed by its children
	// that have a depth of 0.

	drawvec out;

	for (size_t ii = rings.size(); ii > 0; ii--) {
		size_t i = ii - 1;

		if (rings[i].depth == 1 &&
		    (rings[i].parent < 0 || rings[rings[i].parent].depth == 0)) {
			for (size_t j = 0; j < rings[i].data.size(); j++) {
				out.push_back(rings[i].data[j]);
			}

			for (size_t j = 0; j < rings[i].children.size(); j++) {
				if (rings[rings[i].children[j]].depth == 0) {
					for (size_t k = 0; k < rings[rings[i].children[j]].data.size(); k++) {
						out.push_back(rings[rings[i].children[j]].data[k]);
					}
				}
			}
		}
	}

	return out;
}

std::vector<drawvec> remove_collinear(std::vector<drawvec> &rings) {
	std::vector<drawvec> out;

	for (size_t i = 0; i < rings.size(); i++) {
		drawvec outring;
		ssize_t best = -1;

		if (rings[i].size() < 4) {
			continue;
		}

		// Exclude duplicated last point
		size_t len = rings[i].size() - 1;
		for (size_t j = 0; j < len; j++) {
			long long ccw =
				rings[i][(j + len - 1) % len].x * rings[i][(j + len - 0) % len].y +
				rings[i][(j + len - 0) % len].x * rings[i][(j + len + 1) % len].y +
				rings[i][(j + len + 1) % len].x * rings[i][(j + len - 1) % len].y -
				rings[i][(j + len - 1) % len].x * rings[i][(j + len + 1) % len].y -
				rings[i][(j + len - 0) % len].x * rings[i][(j + len - 1) % len].y -
				rings[i][(j + len + 1) % len].x * rings[i][(j + len - 0) % len].y;

			if (ccw != 0) {
				outring.push_back(rings[i][j]);

				if (best < 0 || rings[i][j] < outring[best]) {
					best = outring.size() - 1;
				}
			}
		}

		// Don't include rings that have degenerated away
		if (outring.size() >= 3) {
			drawvec outring2;

			for (size_t j = 0; j < outring.size(); j++) {
				outring2.push_back(outring[(j + best) % outring.size()]);
			}

			outring2.push_back(outring2[0]);
			out.push_back(outring2);
		}
	}

	return out;
}

void spew(drawvec ring, size_t start, size_t end) {
	for (size_t i = start; i < end; i++) {
		fprintf(stderr, " %lld,%lld", ring[i % ring.size()].x, ring[i % ring.size()].y);
	}
	fprintf(stderr, "\n");
}

void find_subrings(drawvec ring, std::vector<drawvec> &rings) {
	if (ring.size() > 0 && ring[0] == ring[ring.size() - 1]) {
		ring.resize(ring.size() - 1);
	}

	while (ring.size() > 0) {
		std::multimap<draw, size_t> seen;

		for (size_t i = 0; i < ring.size(); i++) {
			seen.insert(std::pair<draw, size_t>(ring[i], i));
		}

		double best_area = INT_MAX;
		ssize_t best_start = -1;
		ssize_t best_end = -1;

		for (size_t i = 0; i < ring.size(); i++) {
			if (seen.count(ring[i]) > 1) {
				auto match = seen.equal_range(ring[i]);

				for (auto mi = match.first; mi != match.second; ++mi) {
					double area = 0;
					size_t end;

					if (mi->second < i) {
						area = fabs(get_area(ring, i, mi->second + ring.size()));
						end = mi->second + ring.size();
					} else {
						area = fabs(get_area(ring, i, mi->second));
						end = mi->second;
					}

					std::multimap<draw, size_t> seen2;

					bool acceptable = true;
					for (size_t j = i; j < end; j++) {
						if (seen2.count(ring[j % ring.size()]) > 0) {
							acceptable = false;
							break;
						}
						seen2.insert(std::pair<draw, size_t>(ring[j % ring.size()], j));
					}

					if (acceptable && area > 0 && area < best_area) {
						best_area = area;
						best_start = i;
						best_end = mi->second;

						// fprintf(stderr, "best: %ld to %ld with %f\n", best_start, best_end, best_area);
					}

#if 0
					if (area > 0) {
						fprintf(stderr, "SUB %lu to %lu: %f\n", i, mi->second, area);
						if (mi->second < i) {
							spew(ring, i, mi->second + ring.size());
						} else {
							spew(ring, i, mi->second);
						}
					}
#endif
				}
			}
		}

		if (best_start >= 0) {
			drawvec dv;

			if (best_start < best_end) {
				for (size_t i = best_start; i < best_end; i++) {
					dv.push_back(ring[i % ring.size()]);
				}
			} else {
				for (size_t i = best_start; i < best_end + ring.size(); i++) {
					dv.push_back(ring[i % ring.size()]);
				}
			}

			if (dv.size() == 0) {
				fprintf(stderr, "can't happen %ld to %ld\n", best_start, best_end);
				exit(EXIT_FAILURE);
			}

			dv.push_back(dv[0]);
			rings.push_back(dv);

			if (best_end > best_start) {
				ring.erase(ring.begin() + best_start, ring.begin() + best_end);
			} else {
				ring.erase(ring.begin() + best_start, ring.end());
				ring.erase(ring.begin(), ring.begin() + best_end);
			}

#if 0
			fprintf(stderr, "now: ");
			spew(ring, 0, ring.size());
#endif
		} else {
			ring.push_back(ring[0]);
			rings.push_back(ring);
			break;
		}
	}
}

std::vector<drawvec> walk_ring(std::vector<drawvec> &segments, size_t i, std::multimap<draw, size_t> const &paths, int sign, std::vector<size_t> &alternatives, std::vector<size_t> &choices) {
	std::vector<size_t> all_intersecting;
	all_intersecting.resize(segments.size());
	for (size_t a = 0; a < all_intersecting.size(); a++) {
		all_intersecting[a] = 0;
	}

	std::multimap<draw, std::pair<draw, draw>> existing;

	std::vector<drawvec> ret;

	drawvec ring;
	std::multimap<draw, size_t> seen;

	ssize_t step = -1;

again:

	size_t initial = ring.size();

	ring.push_back(segments[i][0]);
	seen.insert(std::pair<draw, size_t>(segments[i][0], 0));
	ring.push_back(segments[i][1]);
	seen.insert(std::pair<draw, size_t>(segments[i][1], 1));
	segments[i].clear();
	all_intersecting[i] = 0;

	double entryang = 999;

	while (ring.size() > 1) {
		step++;

		if (step >= alternatives.size()) {
			alternatives.resize(step + 1);
			alternatives[step] = 0;
		}
		if (step >= choices.size()) {
			choices.resize(step + 1);
			choices[step] = 0;
		}

#if 0
		if (seen.size() != ring.size()) {
			fprintf(stderr, "mismatched size: seen %lu, ring %lu\n", seen.size(), ring.size());
			for (auto mi = seen.begin(); mi != seen.end(); ++mi) {
				fprintf(stderr, "seen: %lld,%lld %lu\n", mi->first.x, mi->first.y, mi->second);
			}
			for (size_t j = 0; j < ring.size(); j++) {
				fprintf(stderr, "ring: %lu %lld,%lld\n", j, ring[j].x, ring[j].y);
			}
			exit(EXIT_FAILURE);
		}
#endif

		auto match = paths.equal_range(ring[ring.size() - 1]);

		draw prev = ring[ring.size() - 2];
		draw here = ring[ring.size() - 1];
		bool found_something = false;
		std::multimap<double, size_t> exits;
		double ang = atan2(prev.y - here.y, prev.x - here.x);

		for (auto mi = match.first; mi != match.second; ++mi) {
			size_t m = mi->second;

			if (segments[m].size() > 0) {
				draw next;

				if (segments[m][0] == here) {
					next = segments[m][1];
					all_intersecting[m] = 1;
				} else {
					next = segments[m][0];
				}

				double ang2 = ang - atan2(next.y - here.y, next.x - here.x);
				while (ang2 < 0) {
					ang2 += 2 * M_PI;
				}

				// fprintf(stderr, "%lld,%lld to %lld,%lld to %lld,%lld %f\n", prev.x, prev.y, here.x, here.y, next.x, next.y, ang2);

				exits.insert(std::pair<double, size_t>(ang2 * sign, m));
			}
		}
		// fprintf(stderr, "\n");

		alternatives[step] = exits.size();
		// fprintf(stderr, "%lu alternatives for %lu\n", alternatives[step], step);

		int depth = 0;

#if 0
				for (auto ei = exits.begin(); ei != exits.end(); ++ei) {
					if (segments[ei->second][1] == here) {
						fprintf(stderr, "XXXX %lld,%lld to %lld,%lld: %f to %lld,%lld\n",
							prev.x, prev.y, here.x, here.y, ei->first,
							segments[ei->second][1].x, segments[ei->second][1].y);
					} else {
						fprintf(stderr, "from %lld,%lld to %lld,%lld: %f to %lld,%lld\n",
							prev.x, prev.y, here.x, here.y, ei->first,
							segments[ei->second][1].x, segments[ei->second][1].y);
					}
				}
#endif

		ssize_t alternative = -1;

		for (auto ei = exits.begin(); ei != exits.end(); ++ei) {
			alternative++;

			if (segments[ei->second][1] == here) {
				// Points inward
				depth++;
			} else if (segments[ei->second][0] == here) {
				depth--;

				if (alternative < choices[step]) {
					// fprintf(stderr, "avoiding step %ld choice %ld as <%lu\n", step, alternative, choices[step]);
					continue;
				}

				if (depth < 0) {
					bool suitable = true;
					double anga = 999, angb = 999;

					auto alsohere = existing.equal_range(ring[ring.size() - 1]);
					for (auto ahi = alsohere.first; ahi != alsohere.second; ++ahi) {
#if 0
						fprintf(stderr, "from %lld,%lld there is also %lld,%lld to %lld,%lld\n",
							ring[ring.size() - 1].x, ring[ring.size() - 1].y,
							ahi->second.first.x, ahi->second.first.y,
							ahi->second.second.x, ahi->second.second.y);
#endif

						anga = ang - atan2(ahi->second.first.y - here.y, ahi->second.first.x - here.x);
						while (anga < 0) {
							anga += 2 * M_PI;
						}

						angb = ang - atan2(ahi->second.second.y - here.y, ahi->second.second.x - here.x);
						while (angb < 0) {
							angb += 2 * M_PI;
						}

						// If we are following a straight line, use the angle that led into the straight line
						if (anga == 0) {
							//fprintf(stderr, "using %f to follow straight line\n", entryang);
							anga = entryang;
						}
						if (angb == 0) {
							// fprintf(stderr, "using %f to follow straight line\n", entryang);
							angb = entryang;
						}

						// fprintf(stderr, "those have angles %f and %f compared to %f for %lld,%lld\n", anga, angb, ei->first, segments[ei->second][1].x, segments[ei->second][1].y);

						if ((anga < ei->first && angb > ei->first) ||
						    (anga > ei->first && angb < ei->first)) {
							// fprintf(stderr, "exit unsuitable because of crossing\n");
							suitable = false;
							break;
						}
					}

					if (suitable) {
						// if we are following a straight line, keep track of the angle that led into the straight line
						if (anga == ei->first) {
							entryang = angb;
							// fprintf(stderr, "entered straight line from %f\n", entryang);
						} else if (angb == ei->first) {
							entryang = anga;
							// fprintf(stderr, "entered straight line from %f\n", entryang);
						}

						all_intersecting[ei->second] = 0;

						existing.insert(std::pair<draw, std::pair<draw, draw>>(here,
												       std::pair<draw, draw>(prev, segments[ei->second][1])));

						ring.push_back(segments[ei->second][1]);

						segments[ei->second].clear();
						found_something = true;
						seen.insert(std::pair<draw, size_t>(ring[ring.size() - 1], ring.size() - 1));

						break;
					}
				}
			}
		}

		if (!found_something) {
			if (ring[ring.size() - 1] != ring[initial]) {
// fprintf(stderr, "couldn't find a way out\n");
#if 0
				for (size_t a = 0; a < ring.size(); a++) {
					fprintf(stderr, "%lld,%lld ", ring[a].x, ring[a].y);
				}
				fprintf(stderr, "\n");
#endif
				printf(".5 setlinewidth\n");
				for (size_t a = 0; a < ring.size(); a++) {
					printf("%lld %lld %s\n", ring[a].x, ring[a].y, a == 0 ? "moveto" : "lineto");
				}
				printf("stroke\n");

				return std::vector<drawvec>();
				exit(EXIT_FAILURE);
			}
			break;
		}
	}

	for (size_t a = 0; a < all_intersecting.size(); a++) {
		if (all_intersecting[a]) {
			fprintf(stderr, "segment %lu was intersected but not used\n", a);

#if 0
		        for (size_t x = 0; x < ring.size(); x++) {
				fprintf(stderr, "%lld,%lld ", ring[x].x, ring[x].y);
		        }
		        fprintf(stderr, "\n");
#endif

// fprintf(stderr, "not used: %lld,%lld to %lld,%lld\n", segments[a][0].x, segments[a][0].y, segments[a][1].x, segments[a][1].y);

#if 0
			// XXX unusably slow

			ret.push_back(ring);
			ring.resize(0);

			i = a;
			goto again;
#endif
		}
	}

	ret.push_back(ring);

	return ret;
}

drawvec clean_polygon(drawvec &geom, bool all_rings) {
	std::vector<drawvec> segments;

	// Note that this assumes that polygons are closed.
	// If they do not duplicate the last point, the last
	// segment will need to be added explicitly.

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			size_t j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op != VT_LINETO) {
					break;
				}
			}

			double area = get_area(geom, i, j);

			for (size_t k = i + 1; k < j; k++) {
				if (geom[k].x != geom[k - 1].x || geom[k].y != geom[k - 1].y) {
					drawvec dv;
					dv.push_back(geom[k - 1]);
					dv.push_back(geom[k]);

					if (area < 0) {
						dv[0].op = -1;
						dv[1].op = -1;
					} else {
						dv[0].op = 1;
						dv[1].op = 1;
					}

					segments.push_back(dv);
				}
			}

			i = j - 1;
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
		if (segments[i].size() > 0 && segments[i][0] == segments[i][1]) {
			segments[i].clear();
		}
	}

	// Look for spikes
	for (size_t i = 0; i < segments.size(); i++) {
		if (segments[i].size() > 0) {
			double angle = atan2(segments[i][1].y - segments[i][0].y, segments[i][1].x - segments[i][0].x);

			auto match = paths.equal_range(segments[i][1]);

			for (auto mi = match.first; mi != match.second; ++mi) {
				size_t m = mi->second;

				if (m != i && segments[m].size() > 0 &&
				    segments[m][0] == segments[i][1] &&
				    segments[m][1] == segments[i][0]) {
					segments[m].clear();
					segments[i].clear();
					break;
				}

				if (segments[m].size() > 0 &&
				    segments[m][0] == segments[i][1]) {
					double angle2 = atan2(segments[m][0].y - segments[m][1].y, segments[m][0].x - segments[m][1].x);

					if (angle2 == angle) {
						fprintf(stderr, "impossible %f %f in %lu %lu\n", angle, angle2, i, m);
						fprintf(stderr, "   %lld,%lld %lld,%lld vs %lld,%lld %lld,%lld\n",
							segments[i][0].x, segments[i][0].y,
							segments[i][1].x, segments[i][1].y,
							segments[m][0].x, segments[m][0].y,
							segments[m][1].x, segments[m][1].y);

						long long ccw =
							segments[i][0].x * segments[i][1].y +
							segments[i][1].x * segments[m][0].y +
							segments[m][0].x * segments[i][0].y -
							segments[i][0].x * segments[m][0].y -
							segments[i][1].x * segments[i][0].y -
							segments[m][0].x * segments[i][1].y;
						fprintf(stderr, "   ccw is %lld\n", ccw);

						bool again = false;
						check_intersection(segments, i, m, again);

						fprintf(stderr, "   afterward %lld,%lld %lld,%lld vs %lld,%lld %lld,%lld\n",
							segments[i][0].x, segments[i][0].y,
							segments[i][1].x, segments[i][1].y,
							segments[m][0].x, segments[m][0].y,
							segments[m][1].x, segments[m][1].y);
					}
				}
			}
		}
	}

	for (size_t i = 0; i < segments.size(); i++) {
		if (segments[i].size() > 0) {
			// fprintf(stderr, "... %lld,%lld to %lld,%lld\n", segments[i][0].x, segments[i][0].y, segments[i][1].x, segments[i][1].y);
		}
	}

	for (size_t i = 0; i < segments.size(); i++) {
		if (segments[i].size() > 0) {
			std::vector<drawvec> before = segments;
			std::vector<size_t> alternatives;
			std::vector<size_t> choices;

			std::vector<drawvec> found;

			while (1) {
				found = walk_ring(segments, i, paths, 1, alternatives, choices);

				if (found.size() == 0) {
					printf("0 setlinewidth\n");
					printf(".5 .setopacityalpha\n");
					for (size_t j = 0; j < before.size(); j++) {
						if (before[j].size() > 0) {
							printf("%lld %lld moveto %lld %lld lineto stroke\n",
							       before[j][0].x, before[j][0].y,
							       before[j][1].x, before[j][1].y);
						}
					}
					printf("showpage\n");

					segments = before;

					for (size_t j = 0; j < choices.size(); j++) {
						if (alternatives[j] > 1) {
							// fprintf(stderr, "step %lu: %ld of %ld\n", j, choices[j], alternatives[j]);
						}
					}

					bool again = false;

					for (ssize_t j = choices.size() - 1; j >= 0; j--) {
						if (alternatives[j] > 1 && choices[j] + 1 < alternatives[j]) {
							choices[j]++;
							fprintf(stderr, "using choice %lu for %ld\n", choices[j], j);
							choices.resize(j + 1);
							alternatives.resize(j + 1);
							again = true;
							break;
						}
					}

					if (!again) {
						fprintf(stderr, "out of alternatives\n");
						exit(EXIT_FAILURE);
					}
				} else {
					break;
				}
			}

			for (size_t j = 0; j < found.size(); j++) {
				find_subrings(found[j], rings);
			}
		}
	}

	rings = remove_collinear(rings);

	return reassemble_rings(rings, all_rings);
}
