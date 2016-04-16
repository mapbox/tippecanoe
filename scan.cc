#include <stdio.h>
#include <vector>
#include <list>
#include <queue>
#include <set>
#include <algorithm>
#include <sqlite3.h>
#include "geometry.hh"
#include "scan.hh"

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

static void dump(drawvec *dv) {
	printf("--> ");
	for (size_t i = 0; i < dv->size(); i++) {
		printf("%lld,%lld ", (*dv)[i].x, (*dv)[i].y);
	}
	printf("\n");
}

static draw get_line_intersection(draw p0, draw p1, draw p2, draw p3) {
        double s1_x = p1.x - p0.x;
        double s1_y = p1.y - p0.y;
        double s2_x = p3.x - p2.x;
        double s2_y = p3.y - p2.y;

        double s, t;
        s = (-s1_y * (p0.x - p2.x) + s1_x * (p0.y - p2.y)) / (-s2_x * s1_y + s1_x * s2_y);
        t = (s2_x * (p0.y - p2.y) - s2_y * (p0.x - p2.x)) / (-s2_x * s1_y + s1_x * s2_y);

	if (s > 0 && s < 1 && t > 0 && t < 1) {
		return draw(VT_LINETO, p0.x + (t * s1_x), p0.y + (t * s1_y));
	} else {
		return draw(-1, -1, -1);
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
	bool again = true;
	while (again) {
		again = false;

		for (ssize_t i1 = dv1->size() - 1; i1 > 0; i1--) {
			for (ssize_t i2 = dv2->size() - 1; i2 > 0; i2--) {
				if ((*dv1)[i1 - 1].y == (*dv1)[i1].y && (*dv2)[i2 - 1].y == (*dv2)[i2].y) {
					// Both horizontal
					// XXX Can this just be a special case of non-vertical not same angle?

					if ((*dv1)[i1].y == (*dv2)[i2].y) {  // and collinear
						long long dv1xmin = min((*dv1)[i1 - 1].x, (*dv1)[i1].x);
						long long dv1xmax = max((*dv1)[i1 - 1].x, (*dv1)[i1].x);

						long long dv2xmin = min((*dv2)[i2 - 1].x, (*dv2)[i2].x);
						long long dv2xmax = max((*dv2)[i2 - 1].x, (*dv2)[i2].x);

						if (dv1xmin == dv2xmin && dv1xmax == dv2xmax) {
							// They are the same
						} else if (dv1xmax <= dv2xmin) {
							// No overlap
						} else if (dv1xmin >= dv2xmax) {
							// No overlap
						} else if (dv1xmax > dv2xmin && dv1xmax < dv2xmax) {
							// right side of 1 is within 2
							dv2->insert(dv2->begin() + i2, draw(VT_LINETO, dv1xmax, (*dv2)[i2].y));
							dump(dv2);
							again = true;
						} else if (dv1xmin > dv2xmin && dv1xmin < dv2xmax) {
							// left side of 1 is within 2
							dv2->insert(dv2->begin() + i2, draw(VT_LINETO, dv1xmin, (*dv2)[i2].y));
							dump(dv2);
							again = true;
						} else if (dv2xmax > dv1xmin && dv2xmax < dv1xmax) {
							// right side of 2 is within 1
							dv1->insert(dv1->begin() + i1, draw(VT_LINETO, dv2xmax, (*dv1)[i1].y));
							dump(dv1);
							again = true;
						} else if (dv2xmin > dv1xmin && dv2xmin < dv1xmax) {
							// left side of 2 is within 1
							dv1->insert(dv1->begin() + i1, draw(VT_LINETO, dv2xmin, (*dv1)[i1].y));
							dump(dv1);
							again = true;
						} else {
							// Can't happen?
							fprintf(stderr, "Can't happen horizontal\n");
						}
					}
				} else if ((*dv1)[i1 - 1].x == (*dv1)[i1].x && (*dv2)[i2 - 1].x == (*dv2)[i2].x) {
					// Both vertical

					if ((*dv1)[i1].x == (*dv2)[i2].x) {  // and collinear
						long long dv1ymin = min((*dv1)[i1 - 1].y, (*dv1)[i1].y);
						long long dv1ymax = max((*dv1)[i1 - 1].y, (*dv1)[i1].y);

						long long dv2ymin = min((*dv2)[i2 - 1].y, (*dv2)[i2].y);
						long long dv2ymax = max((*dv2)[i2 - 1].y, (*dv2)[i2].y);

						if (dv1ymin == dv2ymin && dv1ymax == dv2ymax) {
							// Thex are the same
						} else if (dv1ymax <= dv2ymin) {
							// No overlap
						} else if (dv1ymin >= dv2ymax) {
							// No overlap
						} else if (dv1ymax > dv2ymin && dv1ymax < dv2ymax) {
							// right side of 1 is within 2
							dv2->insert(dv2->begin() + i2, draw(VT_LINETO, (*dv2)[i2].x, dv1ymax));
							dump(dv2);
							again = true;
						} else if (dv1ymin > dv2ymin && dv1ymin < dv2ymax) {
							// left side of 1 is within 2
							dv2->insert(dv2->begin() + i2, draw(VT_LINETO, (*dv2)[i2].x, dv1ymin));
							dump(dv2);
							again = true;
						} else if (dv2ymax > dv1ymin && dv2ymax < dv1ymax) {
							// right side of 2 is within 1
							dv1->insert(dv1->begin() + i1, draw(VT_LINETO, (*dv1)[i1].x, dv2ymax));
							dump(dv1);
							again = true;
						} else if (dv2ymin > dv1ymin && dv2ymin < dv1ymax) {
							// left side of 2 is within 1
							dv1->insert(dv1->begin() + i1, draw(VT_LINETO, (*dv1)[i1].x, dv2ymin));
							dump(dv1);
							again = true;
						} else {
							// Can't happen?
							fprintf(stderr, "Can't happen vertical\n");
						}
					}
				} else if (((double) (*dv1)[i1].y - (*dv1)[i1 - 1].y) / ((*dv1)[i1].x - (*dv1)[i1 - 1].x) ==
					   ((double) (*dv2)[i2].y - (*dv2)[i2 - 1].y) / ((*dv2)[i2].x - (*dv2)[i2 - 1].x)) {
					// Collinear or parallel at some inconvenient angle

#if 0
					printf("%f == %f\n", (((double) (*dv1)[i1].y - (*dv1)[i1 - 1].y) / ((*dv1)[i1].x - (*dv1)[i1 - 1].x)),
					       ((double) (*dv2)[i2].y - (*dv2)[i2 - 1].y) / ((*dv2)[i2].x - (*dv2)[i2 - 1].x));
#endif

					double slope = ((double) (*dv1)[i1].y - (*dv1)[i1 - 1].y) / ((*dv1)[i1].x - (*dv1)[i1 - 1].x);
					double b1 = (*dv1)[i1].y - (slope * (*dv1)[i1].x);
					double b2 = (*dv2)[i2].y - (slope * (*dv2)[i2].x);
					if (b1 == b2) {
						// Collinear, so we can do the same checks as if they were horizontal and collinear

						long long dv1xmin = min((*dv1)[i1 - 1].x, (*dv1)[i1].x);
						long long dv1xmax = max((*dv1)[i1 - 1].x, (*dv1)[i1].x);

						long long dv2xmin = min((*dv2)[i2 - 1].x, (*dv2)[i2].x);
						long long dv2xmax = max((*dv2)[i2 - 1].x, (*dv2)[i2].x);

						if (dv1xmin == dv2xmin && dv1xmax == dv2xmax) {
							// They are the same
						} else if (dv1xmax <= dv2xmin) {
							// No overlap
						} else if (dv1xmin >= dv2xmax) {
							// No overlap
						} else if (dv1xmax > dv2xmin && dv1xmax < dv2xmax) {
							// right side of 1 is within 2
							dv2->insert(dv2->begin() + i2, draw(VT_LINETO, dv1xmax, (*dv2)[i2 - 1].y + (dv1xmax - (*dv2)[i2 - 1].x) * slope));
							printf("collinear: 1 %f %f\n", b1, slope);
							dump(dv2);
							again = true;
						} else if (dv1xmin > dv2xmin && dv1xmin < dv2xmax) {
							// left side of 1 is within 2
							dv2->insert(dv2->begin() + i2, draw(VT_LINETO, dv1xmin, (*dv2)[i2 - 1].y + (dv1xmin - (*dv2)[i2 - 1].x) * slope));
							printf("collinear: 2 %f %f\n", b1, slope);
							dump(dv2);
							again = true;
						} else if (dv2xmax > dv1xmin && dv2xmax < dv1xmax) {
							// right side of 2 is within 1
							dv1->insert(dv1->begin() + i1, draw(VT_LINETO, dv2xmax, (*dv1)[i1 - 1].y + (dv2xmax - (*dv1)[i1 - 1].x) * slope));
							printf("collinear: 3 %f %f\n", b1, slope);
							dump(dv1);
							again = true;
						} else if (dv2xmin > dv1xmin && dv2xmin < dv1xmax) {
							// left side of 2 is within 1
							dv1->insert(dv1->begin() + i1, draw(VT_LINETO, dv2xmin, (*dv1)[i1 - 1].y + (dv2xmin - (*dv1)[i1 - 1].x) * slope));
							printf("collinear: 4 %f %f\n", b1, slope);
							dump(dv1);
							again = true;
						} else {
							// Can't happen?
							fprintf(stderr, "Can't happen diagonal\n");
						}
					}
				} else {
					draw intersection = get_line_intersection((*dv1)[i1 - 1], (*dv1)[i1], (*dv2)[i2 - 1], (*dv2)[i2]);
					if (intersection.op != -1) {
						dv1->insert(dv1->begin() + i1, intersection);
						dv2->insert(dv2->begin() + i2, intersection);
						printf("intersected:\n");
						dump(dv1);
						dump(dv2);
						again = true;
					}
				}
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

#if 0
	for (size_t i = 0; i < geom.size(); i++) {
		for (size_t j = 0; j < segs[i].size(); j++) {
			printf("%lld,%lld ", segs[i][j].x, segs[i][j].y);
		}
		printf("\n");
	}
	printf("\n");
#endif

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
