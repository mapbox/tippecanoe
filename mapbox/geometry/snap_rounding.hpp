#include <mapbox/geometry/geometry.hpp>
#include <math.h>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <cmath>

namespace mapbox {
namespace geometry {

template <typename T>
void add_vertical(size_t intermediate, size_t which_end, size_t into, std::vector<std::vector<point<T>>> &segments, bool &again, std::vector<size_t> &nexts) {
	again = true;
	std::vector<point<T>> dv;
	dv.push_back(segments[intermediate][which_end]);
	dv.push_back(segments[into][1]);
	segments.push_back(dv);
	segments[into][1] = segments[intermediate][which_end];
	nexts.push_back(nexts[into]);
	nexts[into] = nexts.size() - 1;
}

template <typename T>
void add_horizontal(size_t intermediate, size_t which_end, size_t into, std::vector<std::vector<point<T>>> &segments, bool &again, std::vector<size_t> &nexts) {
	again = true;

	T x = segments[intermediate][which_end].x;
	T y = segments[intermediate][0].y +
	      (segments[intermediate][which_end].x - segments[intermediate][0].x) *
		      (segments[intermediate][1].y - segments[intermediate][0].y) /
		      (segments[intermediate][1].x - segments[intermediate][0].x);
	point<T> d(x, y);

	std::vector<point<T>> dv;
	dv.push_back(d);
	dv.push_back(segments[into][1]);
	segments.push_back(dv);
	segments[into][1] = d;
	nexts.push_back(nexts[into]);
	nexts[into] = nexts.size() - 1;
}

template <typename T>
void warn(std::vector<std::vector<point<T>>> &segments, size_t a, size_t b, bool do_warn) {
	if (do_warn) {
		fprintf(stderr, "%lld,%lld to %lld,%lld intersects %lld,%lld to %lld,%lld\n",
			(long long) segments[a][0].x, (long long) segments[a][0].y,
			(long long) segments[a][1].x, (long long) segments[a][1].y,
			(long long) segments[b][0].x, (long long) segments[b][0].y,
			(long long) segments[b][1].x, (long long) segments[b][1].y);
	}
}

template <typename T>
void check_intersection(std::vector<std::vector<point<T>>> &segments, size_t a, size_t b, bool &again, std::vector<size_t> &nexts, bool do_warn, bool endpoint_ok) {
	T s10_x = segments[a][1].x - segments[a][0].x;
	T s10_y = segments[a][1].y - segments[a][0].y;
	T s32_x = segments[b][1].x - segments[b][0].x;
	T s32_y = segments[b][1].y - segments[b][0].y;

	// http://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
	T denom = s10_x * s32_y - s32_x * s10_y;

	if (denom == 0) {
		// They are parallel or collinear. Find out if they are collinear.
		// http://www.cpsc.ucalgary.ca/~marina/papers/Segment_intersection.ps

		T ccw =
			segments[a][0].x * segments[a][1].y +
			segments[a][1].x * segments[b][0].y +
			segments[b][0].x * segments[a][0].y -
			segments[a][0].x * segments[b][0].y -
			segments[a][1].x * segments[a][0].y -
			segments[b][0].x * segments[a][1].y;

		if (ccw == 0) {
			if (segments[a][0].x == segments[a][1].x) {
				// Vertical

				T amin, amax, bmin, bmax;
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
					warn(segments, a, b, do_warn);
					add_vertical(b, 0, a, segments, again, nexts);
				}
				if (segments[b][1].y > amin && segments[b][1].y < amax) {
					// B1 is in A
					warn(segments, a, b, do_warn);
					add_vertical(b, 1, a, segments, again, nexts);
				}
				if (segments[a][0].y > bmin && segments[a][0].y < bmax) {
					// A0 is in B
					warn(segments, a, b, do_warn);
					add_vertical(a, 0, b, segments, again, nexts);
				}
				if (segments[a][1].y > bmin && segments[a][1].y < bmax) {
					// A1 is in B
					warn(segments, a, b, do_warn);
					add_vertical(a, 1, b, segments, again, nexts);
				}
			} else {
				// Horizontal or diagonal

				T amin, amax, bmin, bmax;
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
					add_horizontal(b, 0, a, segments, again, nexts);
					warn(segments, a, b, do_warn);
				} else if (segments[b][1].x > amin && segments[b][1].x < amax) {
					// B1 is in A
					add_horizontal(b, 1, a, segments, again, nexts);
					warn(segments, a, b, do_warn);
				} else if (segments[a][0].x > bmin && segments[a][0].x < bmax) {
					// A0 is in B
					warn(segments, a, b, do_warn);
					add_horizontal(a, 0, b, segments, again, nexts);
				} else if (segments[a][1].x > bmin && segments[a][1].x < bmax) {
					// A1 is in B
					warn(segments, a, b, do_warn);
					add_horizontal(a, 1, b, segments, again, nexts);
				}
			}
		}
	} else {
		// Neither parallel nor collinear, so may intersect at a single point

		T s02_x = segments[a][0].x - segments[b][0].x;
		T s02_y = segments[a][0].y - segments[b][0].y;

		double s = (s10_x * s02_y - s10_y * s02_x) / (long double) denom;
		double t = (s32_x * s02_y - s32_y * s02_x) / (long double) denom;

		if (t >= 0 && t <= 1 && s >= 0 && s <= 1) {
			T x = (T) round(segments[a][0].x + t * s10_x);
			T y = (T) round(segments[a][0].y + t * s10_y);

			if ((t > 0 && t < 1 && s > 0 && s < 1) || !endpoint_ok) {
				if (t >= 0 && t <= 1) {
					if ((x != segments[a][0].x || y != segments[a][0].y) && (x != segments[a][1].x || y != segments[a][1].y)) {
						warn(segments, a, b, do_warn);
						// splitting a
						std::vector<point<T>> dv;
						dv.push_back(point<T>(x, y));
						dv.push_back(segments[a][1]);
						segments.push_back(dv);
						segments[a][1] = point<T>(x, y);
						nexts.push_back(nexts[a]);
						nexts[a] = nexts.size() - 1;
						again = true;
					}
				}

				if (s >= 0 && s <= 1) {
					if ((x != segments[b][0].x || y != segments[b][0].y) && (x != segments[b][1].x || y != segments[b][1].y)) {
						// splitting b
						warn(segments, a, b, do_warn);
						std::vector<point<T>> dv;
						dv.push_back(point<T>(x, y));
						dv.push_back(segments[b][1]);
						segments.push_back(dv);
						segments[b][1] = point<T>(x, y);
						nexts.push_back(nexts[b]);
						nexts[b] = nexts.size() - 1;
						again = true;
					}
				}
			}
		}
	}
}

template <typename T>
void partition(std::vector<std::vector<point<T>>> &segs, std::vector<size_t> &subset, int direction, std::set<std::pair<size_t, size_t>> &possible) {
	std::vector<T> points;

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

	if (points.size() == 0) {
		return;
	}

	size_t mid = points.size() / 2;
	std::nth_element(points.begin(), points.begin() + mid, points.end());
	T median = points[mid];

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

template <typename T>
std::vector<std::vector<point<T>>> intersect_segments(std::vector<std::vector<point<T>>> segments, std::vector<size_t> &nexts, bool do_warn, bool endpoint_ok) {
	bool again = true;

	while (again) {
		again = false;

		std::set<std::pair<size_t, size_t>> possible;

		std::vector<size_t> subset;
		for (size_t i = 0; i < segments.size(); i++) {
			subset.push_back(i);
		}

		partition(segments, subset, 0, possible);

		for (auto it = possible.begin(); it != possible.end(); ++it) {
			check_intersection(segments, it->first, it->second, again, nexts, do_warn, endpoint_ok);
		}
	}

	return segments;
}

template <typename T>
linear_ring<T> remove_collinear(linear_ring<T> ring) {
	linear_ring<T> out;

	size_t len = ring.size() - 1;  // Exclude duplicated last point
	for (size_t j = 0; j < len; j++) {
		long long ccw =
			ring[(j + len - 1) % len].x * ring[(j + len - 0) % len].y +
			ring[(j + len - 0) % len].x * ring[(j + len + 1) % len].y +
			ring[(j + len + 1) % len].x * ring[(j + len - 1) % len].y -
			ring[(j + len - 1) % len].x * ring[(j + len + 1) % len].y -
			ring[(j + len - 0) % len].x * ring[(j + len - 1) % len].y -
			ring[(j + len + 1) % len].x * ring[(j + len - 0) % len].y;

		if (ccw != 0) {
			out.push_back(ring[j]);
		}

		if (ring.size() > 0 && ring[0] != ring[ring.size() - 1]) {
			ring.push_back(ring[0]);
		}
	}

	return out;
}

template <typename T>
multi_polygon<T> snap_round(multi_polygon<T> geom, bool do_warn, bool endpoint_ok) {
	std::vector<std::vector<point<T>>> segments;
	std::vector<size_t> nexts;
	std::vector<std::vector<size_t>> ring_starts;

	// Crunch out any 0-length segments
	for (size_t i = 0; i < geom.size(); i++) {
		for (size_t j = 0; j < geom[i].size(); j++) {
			for (ssize_t k = geom[i][j].size() - 1; k > 0; k--) {
				if (geom[i][j][k] == geom[i][j][k - 1]) {
					geom[i][j].erase(geom[i][j].begin() + k);
				}
			}
		}
	}

	for (size_t i = 0; i < geom.size(); i++) {
		ring_starts.push_back(std::vector<size_t>());

		for (size_t j = 0; j < geom[i].size(); j++) {
			size_t s = geom[i][j].size();

			if (s > 1) {
				ring_starts[i].push_back(segments.size());
				size_t first = nexts.size();

				for (size_t k = 0; k + 1 < s; k++) {
					std::vector<point<T>> dv;
					dv.push_back(geom[i][j][k]);
					dv.push_back(geom[i][j][k + 1]);

					segments.push_back(dv);
					nexts.push_back(nexts.size() + 1);
				}

				// Fabricate a point if ring was not closed
				if (geom[i][j][0] != geom[i][j][s - 1]) {
					std::vector<point<T>> dv;
					dv.push_back(geom[i][j][s - 1]);
					dv.push_back(geom[i][j][0]);

					segments.push_back(dv);
					nexts.push_back(nexts.size() + 1);
				}

				// Last point of ring points back to first
				nexts[nexts.size() - 1] = first;
			}
		}
	}

	segments = intersect_segments(segments, nexts, do_warn, endpoint_ok);

	multi_polygon<T> mp;
	for (size_t i = 0; i < ring_starts.size(); i++) {
		mp.push_back(polygon<T>());

		for (size_t j = 0; j < ring_starts[i].size(); j++) {
			mp[i].push_back(linear_ring<T>());

			size_t k = ring_starts[i][j];
			do {
				mp[i][j].push_back(segments[k][0]);
				k = nexts[k];
			} while (k != ring_starts[i][j]);

			mp[i][j].push_back(segments[ring_starts[i][j]][0]);
		}
	}

	return mp;
}

template <typename T>
multi_line_string<T> snap_round(multi_line_string<T> geom, bool do_warn, bool endpoint_ok) {
	std::vector<std::vector<point<T>>> segments;
	std::vector<size_t> nexts;
	std::vector<size_t> ring_starts;

	// Crunch out any 0-length segments
	for (size_t j = 0; j < geom.size(); j++) {
		for (ssize_t k = geom[j].size() - 1; k > 0; k--) {
			if (geom[j][k] == geom[j][k - 1]) {
				geom[j].erase(geom[j].begin() + k);
			}
		}
	}

	for (size_t j = 0; j < geom.size(); j++) {
		size_t s = geom[j].size();

		if (s > 1) {
			ring_starts.push_back(segments.size());
			size_t first = nexts.size();

			for (size_t k = 0; k + 1 < s; k++) {
				std::vector<point<T>> dv;
				dv.push_back(geom[j][k]);
				dv.push_back(geom[j][k + 1]);

				segments.push_back(dv);
				nexts.push_back(nexts.size() + 1);
			}

			// Last point of ring points back to first
			nexts[nexts.size() - 1] = first;
		}
	}

	segments = intersect_segments(segments, nexts, do_warn, endpoint_ok);

	multi_line_string<T> mp;
	for (size_t j = 0; j < ring_starts.size(); j++) {
		mp.push_back(line_string<T>());

		size_t k = ring_starts[j];
		size_t last = k;
		do {
			mp[j].push_back(segments[k][0]);
			last = k;
			k = nexts[k];
		} while (k != ring_starts[j]);

		mp[j].push_back(segments[last][1]);
	}

	return mp;
}
}
}
