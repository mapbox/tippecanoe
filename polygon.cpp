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
	segments.push_back(dv);
	segments[into][1] = segments[intermediate][which_end];
}

void add_horizontal(size_t intermediate, size_t which_end, size_t into, std::vector<drawvec> &segments, bool &again) {
	again = true;

	long long x = segments[intermediate][which_end].x;
	long long y = segments[intermediate][0].y +
		      (segments[intermediate][which_end].x - segments[intermediate][0].x) *
			      (segments[intermediate][1].y - segments[intermediate][0].y) /
			      (segments[intermediate][1].x - segments[intermediate][0].x);
	draw d(VT_LINETO, x, y);

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

		if (t >= 0 && t <= 1 && s >= 0 && s <= 1) {
			long long x = round(segments[a][0].x + t * s10_x);
			long long y = round(segments[a][0].y + t * s10_y);

			if (t > 0 && t < 1) {
				if ((x != segments[a][0].x || y != segments[a][0].y) && (x != segments[a][1].x || y != segments[a][1].y)) {
					// splitting a
					drawvec dv;
					dv.push_back(draw(VT_MOVETO, x, y));
					dv.push_back(segments[a][1]);
					segments.push_back(dv);
					segments[a][1] = draw(VT_LINETO, x, y);
					again = true;
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
					again = true;
				}
			}
		}
	}
}

std::vector<drawvec> intersect_segments(std::vector<drawvec> segments) {
	bool again = true;

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

		for (auto it = possible.begin(); it != possible.end(); ++it) {
			check_intersection(segments, it->first, it->second, again);
		}
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

drawvec reassemble_rings(std::vector<drawvec> &orings) {
	// Index points by ring so we can find out which points appear in only one ring
	std::multimap<draw, size_t> point_to_ring;
	for (size_t i = 0; i < orings.size(); i++) {
		for (size_t j = 0; j < orings[i].size(); j++) {
			if (j == 0) {
				orings[i][j].op = VT_MOVETO;
			} else {
				orings[i][j].op = VT_LINETO;
				// skip 0 because the last point of each ring duplicates the first
				point_to_ring.insert(std::pair<draw, size_t>(orings[i][j], i));
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
		ssize_t unique_point = -1;

		for (size_t k = 0; k < rings[i].data.size(); k++) {
			if (point_to_ring.count(rings[i].data[k]) == 1) {
				unique_point = k;
				break;
			}
		}

		double xx, yy;

		if (unique_point >= 0) {
			xx = rings[i].data[unique_point].x;
			yy = rings[i].data[unique_point].y;
		} else {
			size_t count = rings[i].data.size() - 1;
			xx = yy = 0;

			for (size_t k = 0; k < count; k++) {
				xx += (double) rings[i].data[k].x / count;
				yy += (double) rings[i].data[k].y / count;
			}

			if (!pnpoly(rings[i].data, 0, rings[i].data.size(), xx, yy)) {
				fprintf(stderr, "Skipping ring with no unique point, centroid %f,%f not within\n", xx, yy);
				continue;
			}
		}

		for (size_t j = i + 1; j < rings.size(); j++) {
			if (pnpoly(rings[j].data, 0, rings[j].data.size(), xx, yy)) {
				rings[i].parent = j;
				rings[j].children.push_back(i);
				break;
			}
		}
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

	// Look for spikes
	for (size_t i = 0; i < segments.size(); i++) {
		if (segments[i].size() > 0) {
			auto match = paths.equal_range(segments[i][1]);
			for (auto mi = match.first; mi != match.second; ++mi) {
				size_t m = mi->second;

				if (segments[m].size() > 0 &&
				    segments[m][0] == segments[i][1] &&
				    segments[m][1] == segments[i][0]) {
					segments[m].clear();
					segments[i].clear();
					break;
				}
			}
		}
	}

	for (size_t i = 0; i < segments.size(); i++) {
		std::map<draw, size_t> seen;

		if (segments[i].size() > 0) {
			drawvec ring;

			ring.push_back(segments[i][0]);
			seen.insert(std::pair<draw, size_t>(segments[i][0], 0));
			ring.push_back(segments[i][1]);
			seen.insert(std::pair<draw, size_t>(segments[i][1], 1));
			segments[i].clear();

			while (ring.size() > 1) {
				auto match = paths.equal_range(ring[ring.size() - 1]);

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
					if (segments[ei->second][1] == here) {
						// Points inward
						depth++;
					} else {
						depth--;
						if (depth < 0) {
							ring.push_back(segments[ei->second][1]);
							segments[ei->second].clear();
							found_something = true;

							auto where_seen = seen.find(ring[ring.size() - 1]);
							if (where_seen != seen.end()) {
								drawvec ring2;
								size_t loop = where_seen->second;

								for (size_t j = loop; j < ring.size(); j++) {
									ring2.push_back(ring[j]);
								}
								rings.push_back(ring2);

								ring.resize(loop + 1);
								seen.clear();
								for (size_t j = 0; j < ring.size(); j++) {
									seen.insert(std::pair<draw, size_t>(ring[j], j));
								}
							} else {
								seen.insert(std::pair<draw, size_t>(ring[ring.size() - 1], ring.size() - 1));
							}

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

			if (ring.size() > 1) {
				rings.push_back(ring);
			}
		}
	}

	rings = remove_collinear(rings);

	return reassemble_rings(rings);
}
