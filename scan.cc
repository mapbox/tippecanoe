#include <stdio.h>
#include <vector>
#include <list>
#include <queue>
#include <set>
#include <algorithm>
#include <sqlite3.h>
#include "geometry.hh"

extern "C" {
#include "tile.h"
};

struct seg {
	long long index;
	drawvec *dv;

	seg(long long _index, drawvec *_dv) {
		index = _index;
		dv = _dv;
	}

	bool operator<(const seg &s) const {
		return index < s.index;
	}
};

long long min(long long a, long long b) {
	if (a < b) {
		return a;
	} else {
		return b;
	}
}

long long max(long long a, long long b) {
	if (a > b) {
		return a;
	} else {
		return b;
	}
}

void check_intersections(drawvec *dv1, drawvec *dv2) {
	// Go through the sub-segments from each ring segment, looking for cases that intersect.
	// If they do intersect, insert a new intermediate point at the intersection.

	// The new point may deflect the segment up to a pixel away from where it was before,
	// but that is what is necessary to keep from having irreconcilable self-intersections.

	// The messy part: after inserting a point, making sure that the movement doesn't
	// cause any new intersections.

	// Don't check a segment against itself, since it's all overlaps
	if (dv1 == dv2) {
		return;
	}

	// Counting down from size - 1 to 1 so that insertions don't change the index.
	// The segment is i-1 and i.
	for (ssize_t i = dv1->size() - 1; i > 0; i--) {
		for (ssize_t j = dv2->size() - 1; j > 0; j--) {
			if ((*dv1)[i - 1].y == (*dv1)[i].y && (*dv2)[i - 1].y == (*dv2)[i].y) {
				// Both horizontal

				long long dv1xmin = min((*dv1)[i - 1].x, (*dv1)[i].x);
				long long dv1xmax = max((*dv1)[i - 1].x, (*dv1)[i].x);

				long long dv2xmin = min((*dv2)[j - 1].x, (*dv2)[j].x);
				long long dv2xmax = max((*dv2)[j - 1].x, (*dv2)[j].x);

				if (dv1xmin == dv2xmin && dv1xmax == dv2xmax) {
					// They are the same
				} else if (dv1xmax <= dv2xmin) {
					// No overlap
				} else if (dv1xmin >= dv2xmax) {
					// No overlap
				} else if (dv1xmin > dv2xmin && dv1xmax < dv2xmax) {
					// 1 contained within 2
				} else if (dv2xmin > dv1xmin && dv2xmax < dv1xmax) {
					// 2 contained within 1
				} else if (dv1xmax > dv2xmin && dv1xmax < dv2xmax) {
					// right side of 1 is within 2
				} else if (dv1xmin > dv2xmin && dv1xmin < dv2xmax) {
					// left side of 1 is within 2
				} else if (dv2xmax > dv1xmin && dv2xmax < dv1xmax) {
					// right side of 2 is within 1
				} else if (dv2xmin > dv1xmin && dv2xmin < dv1xmax) {
					// left side of 2 is within 1
				} else {
					// Can't happen?
				}
			} else if ((*dv1)[i - 1].x == (*dv1)[i].x && (*dv2)[i - 1].x == (*dv2)[i].x) {
				// Both vertical
			} else if (1 /* XXX */) {
				// Collinear at some inconvenient angle
			} else {

			}
		}
	}
}

void scan(drawvec &geom) {
	// Following the Bentley-Ottmann sweep-line concept,
	// but less clever about the active set

	drawvec *segs = new drawvec[geom.size()];

	std::vector<long long> endpoints;
	std::vector<seg> lefts;
	std::vector<seg> rights;

	// Make an ordered list of endpoints
	// Make a list of segments ordered by left side
	// Make a list of segments ordered by right side

	size_t n = 0;
	for (size_t i = 0; i < geom.size() - 1; i++) {
		if (geom[i + 1].op == VT_LINETO) {
			// There will be duplicates, but we'll skip over them
			endpoints.push_back(geom[i].x);
			endpoints.push_back(geom[i + 1].x);

			drawvec dv;
			if (geom[i].x < geom[i + 1].x || (geom[i].x == geom[i + 1].x && geom[i].y < geom[i + 1].y)) {
				dv.push_back(geom[i]);
				dv.push_back(geom[i + 1]);
			} else {
				dv.push_back(geom[i + 1]);
				dv.push_back(geom[i]);
			}

			segs[n] = dv;

			if (geom[i].x < geom[i + 1].x) {
				lefts.push_back(seg(geom[i].x, &segs[n]));
				rights.push_back(seg(geom[i + 1].x, &segs[n]));
			} else {
				lefts.push_back(seg(geom[i + 1].x, &segs[n]));
				rights.push_back(seg(geom[i].x, &segs[n]));
			}

			n++;
		}
	}

	// Run through the endpoints
	// At each:
	//	Add the ones that start there to the active set
	//	Check the active set for intersections
	//	Remove the ones that end there from the active set

	std::set<drawvec *> active;

	std::sort(endpoints.begin(), endpoints.end());
	std::sort(lefts.begin(), lefts.end());
	std::sort(rights.begin(), rights.end());

	for (size_t i = 0; i < endpoints.size(); i++) {
		// Skip over duplicate endpoints
		while (i + 1 < endpoints.size() && endpoints[i] == endpoints[i + 1]) {
			i++;
		}
		seg here(i, NULL);

		// Add the segments that start here to the active set

		std::pair<std::vector<seg>::iterator, std::vector<seg>::iterator> starting =
			std::equal_range(lefts.begin(), lefts.end(), here);
		for (std::vector<seg>::iterator it = starting.first; it != starting.second; it++) {
			active.insert(it->dv);
		}

		// Check the active set for intersections

		for (std::set<drawvec *>::iterator it = active.begin(); it != active.end(); it++) {
			for (std::set<drawvec *>::iterator it2 = active.begin(); it2 != active.end(); it2++) {
				check_intersections(*it, *it2);
			}
		}

		// Remove the segments that end here from the active set

		std::pair<std::vector<seg>::iterator, std::vector<seg>::iterator> ending =
			std::equal_range(rights.begin(), rights.end(), here);
		for (std::vector<seg>::iterator it = ending.first; it != ending.second; it++) {
			active.erase(it->dv);
		}
	}

	// At this point we have a whole lot of polygon edges and need to reconstruct
	// polygons from them.

	// First, remove all exact duplicates, since those are places where an inner ring
	// exactly matches up to the edge of an outer ring, and they need to be cut down
	// to just one ring.

	// Then index each remaining segment by its start and endpoint.

	// Use that to reconstruct rings:
	// From each arbitrary starting point, follow the right hand rule to form a ring from it,
	// removing those segments from further consideration.

	// Then afterward, use point-in-polygon tests to figure out which rings are inside
	// which other rings.
}
