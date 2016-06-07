#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <vector>
#include <list>
#include <queue>
#include <set>
#include <map>
#include <algorithm>
#include <sqlite3.h>
#include "geometry.hpp"
#include "scan.hpp"
#include "tile.hpp"

struct loc {
	long long x;
	long long y;
	int direction;

	bool operator<(const loc &other) const {
		if (x < other.x) {
			return true;
		}
		if (x == other.x && y < other.y) {
			return true;
		}

		return false;
	}

	loc(const draw &d, int _direction) {
		x = d.x;
		y = d.y;
		direction = _direction;
	}

	loc() {
	}
};

static long long min(long long a, long long b) {
	if (a < b) {
		return a;
	} else {
		return b;
	}
}

static long long max(long long a, long long b) {
	if (a > b) {
		return a;
	} else {
		return b;
	}
}

static void dump(drawvec *dv) {
	fprintf(stderr, "--> ");
	for (size_t i = 0; i < dv->size(); i++) {
		fprintf(stderr, "%lld,%lld ", (*dv)[i].x, (*dv)[i].y);
	}
	fprintf(stderr, "\n");
}

static draw get_line_intersection(draw p0, draw p1, draw p2, draw p3) {
	double s1_x = p1.x - p0.x;
	double s1_y = p1.y - p0.y;
	double s2_x = p3.x - p2.x;
	double s2_y = p3.y - p2.y;

	if (max(p0.y, p1.y) < min(p2.y, p3.y)) {
		return draw(-1, -1, -1);
	}
	if (min(p0.y, p1.y) > max(p2.y, p3.y)) {
		return draw(-1, -1, -1);
	}

	if (-s2_x * s1_y + s1_x * s2_y == 0) {
		fprintf(stderr, "Dividing by 0: %lld,%lld to %lld,%lld and %lld,%lld to %lld,%lld\n",
			p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
	}

	double s, t;
	s = (-s1_y * (p0.x - p2.x) + s1_x * (p0.y - p2.y)) / (-s2_x * s1_y + s1_x * s2_y);

	if (s < 0 || s > 1) {
		return draw(-1, -1, -1);
	}

	t = (s2_x * (p0.y - p2.y) - s2_y * (p0.x - p2.x)) / (-s2_x * s1_y + s1_x * s2_y);

	// Include it if the intersection is at the endpoint of either line but not both
	if ((s == 0 || s == 1) && (t > 0 && t < 1)) {
		return draw(VT_LINETO, round(p0.x + (t * s1_x)), round(p0.y + (t * s1_y)));
	} else if ((t == 0 || t == 1) && (s > 0 && s < 1)) {
		return draw(VT_LINETO, round(p0.x + (t * s1_x)), round(p0.y + (t * s1_y)));
	} else if (s > 0 && s < 1 && t > 0 && t < 1) {
		return draw(VT_LINETO, round(p0.x + (t * s1_x)), round(p0.y + (t * s1_y)));
	} else {
		return draw(-1, -1, -1);
	}
}

static bool check_intersections(drawvec *dv1, drawvec *dv2) {
	// Go through the sub-segments from each ring segment, looking for cases that intersect.
	// If they do intersect, insert a new intermediate point at the intersection.

	// The new point may deflect the segment up to a pixel away from where it was before,
	// but that is what is necessary to keep from having irreconcilable self-intersections.

	// The messy part: after inserting a point, making sure that the movement doesn't
	// cause any new intersections. So recheck after making a change.

	bool did_something = false;

	// Don't check a segment against itself, since it's all overlaps
	if (dv1 == dv2) {
		return did_something;
	}

	if (min((*dv1)[0].y, (*dv1)[dv1->size() - 1].y) > max((*dv2)[0].y, (*dv2)[dv2->size() - 1].y)) {
		return false;
	}
	if (max((*dv1)[0].y, (*dv1)[dv1->size() - 1].y) < min((*dv2)[0].y, (*dv2)[dv2->size() - 1].y)) {
		return false;
	}
	if (min((*dv2)[0].y, (*dv2)[dv2->size() - 1].y) > max((*dv1)[0].y, (*dv1)[dv1->size() - 1].y)) {
		return false;
	}
	if (max((*dv2)[0].y, (*dv2)[dv2->size() - 1].y) < min((*dv1)[0].y, (*dv1)[dv1->size() - 1].y)) {
		return false;
	}

	// Counting down from size - 1 to 1 so that insertions don't change the index.
	// The segment is i-1 and i.
	bool again = true;
	while (again) {
		again = false;

		for (ssize_t i1 = dv1->size() - 1; i1 > 0; i1--) {
			for (ssize_t i2 = dv2->size() - 1; i2 > 0; i2--) {
				if ((*dv1)[i1 - 1].x == (*dv1)[i1].x && (*dv1)[i1 - 1].y == (*dv1)[i1].y) {
					// 0-length first segment
				} else if ((*dv2)[i2 - 1].x == (*dv2)[i2].x && (*dv2)[i2 - 1].y == (*dv2)[i2].y) {
					// 0-length second segment
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
							if (dv1ymin > dv2ymin && dv1ymin < dv2ymax) {
								// all of 1 is within 2
								if ((*dv2)[i2 - 1].y < (*dv2)[i2].y) {
									dv2->insert(dv2->begin() + i2, draw(VT_LINETO, (*dv2)[i2].x, dv1ymin));
									dv2->insert(dv2->begin() + i2, draw(VT_LINETO, (*dv2)[i2].x, dv1ymax));
								} else {
									dv2->insert(dv2->begin() + i2, draw(VT_LINETO, (*dv2)[i2].x, dv1ymax));
									dv2->insert(dv2->begin() + i2, draw(VT_LINETO, (*dv2)[i2].x, dv1ymin));
								}
								again = true;
							} else {
								// right side of 1 is within 2
								dv2->insert(dv2->begin() + i2, draw(VT_LINETO, (*dv2)[i2].x, dv1ymax));
								again = true;
							}
						} else if (dv1ymin > dv2ymin && dv1ymin < dv2ymax) {
							// left side of 1 is within 2
							dv2->insert(dv2->begin() + i2, draw(VT_LINETO, (*dv2)[i2].x, dv1ymin));
							again = true;
						} else if (dv2ymax > dv1ymin && dv2ymax < dv1ymax) {
							if (dv2ymin > dv1ymin && dv2ymin < dv1ymax) {
								// all of 2 is within 1
								if ((*dv1)[i1 - 1].y < (*dv1)[i1].y) {
									dv1->insert(dv1->begin() + i1, draw(VT_LINETO, (*dv1)[i1].x, dv2ymin));
									dv1->insert(dv1->begin() + i1, draw(VT_LINETO, (*dv1)[i1].x, dv2ymax));
								} else {
									dv1->insert(dv1->begin() + i1, draw(VT_LINETO, (*dv1)[i1].x, dv2ymax));
									dv1->insert(dv1->begin() + i1, draw(VT_LINETO, (*dv1)[i1].x, dv2ymin));
								}
								again = true;
							} else {
								// right side of 2 is within 1
								dv1->insert(dv1->begin() + i1, draw(VT_LINETO, (*dv1)[i1].x, dv2ymax));
								again = true;
							}
						} else if (dv2ymin > dv1ymin && dv2ymin < dv1ymax) {
							// left side of 2 is within 1
							dv1->insert(dv1->begin() + i1, draw(VT_LINETO, (*dv1)[i1].x, dv2ymin));
							again = true;
						} else {
							// Can't happen?
							fprintf(stderr, "Can't happen vertical\n");
						}
					}
				} else if (((double) (*dv1)[i1].y - (*dv1)[i1 - 1].y) / ((*dv1)[i1].x - (*dv1)[i1 - 1].x) ==
					   ((double) (*dv2)[i2].y - (*dv2)[i2 - 1].y) / ((*dv2)[i2].x - (*dv2)[i2 - 1].x)) {
#if 0
					printf("%f == %f\n", (((double) (*dv1)[i1].y - (*dv1)[i1 - 1].y) / ((*dv1)[i1].x - (*dv1)[i1 - 1].x)),
					       ((double) (*dv2)[i2].y - (*dv2)[i2 - 1].y) / ((*dv2)[i2].x - (*dv2)[i2 - 1].x));
#endif
					// Collinear or parallel horizontally or at some inconvenient angle

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
							dv2->insert(dv2->begin() + i2, draw(VT_LINETO, dv1xmax, round((*dv2)[i2 - 1].y + (dv1xmax - (*dv2)[i2 - 1].x) * slope)));
							again = true;
						} else if (dv1xmin > dv2xmin && dv1xmin < dv2xmax) {
							// left side of 1 is within 2
							dv2->insert(dv2->begin() + i2, draw(VT_LINETO, dv1xmin, round((*dv2)[i2 - 1].y + (dv1xmin - (*dv2)[i2 - 1].x) * slope)));
							again = true;
						} else if (dv2xmax > dv1xmin && dv2xmax < dv1xmax) {
							// right side of 2 is within 1
							dv1->insert(dv1->begin() + i1, draw(VT_LINETO, dv2xmax, round((*dv1)[i1 - 1].y + (dv2xmax - (*dv1)[i1 - 1].x) * slope)));
							again = true;
						} else if (dv2xmin > dv1xmin && dv2xmin < dv1xmax) {
							// left side of 2 is within 1
							dv1->insert(dv1->begin() + i1, draw(VT_LINETO, dv2xmin, round((*dv1)[i1 - 1].y + (dv2xmin - (*dv1)[i1 - 1].x) * slope)));
							again = true;
						} else {
							// Can't happen?
							fprintf(stderr, "Can't happen diagonal\n");
						}
					}
				} else {
					draw intersection = get_line_intersection((*dv1)[i1 - 1], (*dv1)[i1], (*dv2)[i2 - 1], (*dv2)[i2]);
					if (intersection.op != -1) {
						if ((intersection.x != (*dv1)[i1 - 1].x || intersection.y != (*dv1)[i1 - 1].y) &&
						    (intersection.x != (*dv1)[i1].x || intersection.y != (*dv1)[i1].y)) {
							dv1->insert(dv1->begin() + i1, intersection);
							again = true;
						}
						if ((intersection.x != (*dv2)[i2 - 1].x || intersection.y != (*dv2)[i2 - 1].y) &&
						    (intersection.x != (*dv2)[i2].x || intersection.y != (*dv2)[i2].y)) {
							dv2->insert(dv2->begin() + i2, intersection);
							again = true;
						}
					}
				}
			}
		}

		if (again) {
			did_something = true;
		}

		// Caller will repeat
		again = false;
	}

	return did_something;
}

int edgecmp(const drawvec &a, const drawvec &b) {
	size_t i;

	for (i = 0; i < a.size() && i < b.size(); i++) {
		if (a[i].x < b[i].x) {
			return -1;
		} else if (a[i].x > b[i].x) {
			return 1;
		} else {
			if (a[i].y < b[i].y) {
				return -1;
			} else if (a[i].y > b[i].y) {
				return 1;
			}
		}
	}

	if (a.size() < b.size()) {
		return -1;
	} else if (a.size() > b.size()) {
		return 1;
	} else {
		return 0;
	}
}

struct s_edgecmp {
	bool operator()(const drawvec &a, const drawvec &b) {
		int cmp = edgecmp(a, b);
		if (cmp < 0) {
			return true;
		} else {
			return false;
		}
	}
} s_edgecmp;

typedef drawvec *dvp;

bool partition(std::vector<drawvec *> segs, int direction) {
	std::vector<long long> points;

	// List of X or Y midpoints of edges, so we can find the median

	if (direction == 0) {
		for (size_t i = 0; i < segs.size(); i++) {
			points.push_back(((*segs[i])[0].x + (*segs[i])[segs[i]->size() - 1].x) / 2);
		}
	} else {
		for (size_t i = 0; i < segs.size(); i++) {
			points.push_back(((*segs[i])[0].y + (*segs[i])[segs[i]->size() - 1].y) / 2);
		}
	}

	std::sort(points.begin(), points.end());
	long long median = points[points.size() / 2];

	// Partition into sets that are above or below, or to the left or to the right of, the median.
	// Segments that cross the median appear in both.

	std::vector<drawvec *> one;
	std::vector<drawvec *> two;

	if (direction == 0) {
		for (size_t i = 0; i < segs.size(); i++) {
			if ((*segs[i])[0].x <= median || (*segs[i])[segs[i]->size() - 1].x <= median) {
				one.push_back(segs[i]);
			}
			if ((*segs[i])[0].x >= median || (*segs[i])[segs[i]->size() - 1].x >= median) {
				two.push_back(segs[i]);
			}
		}
	} else {
		for (size_t i = 0; i < segs.size(); i++) {
			if ((*segs[i])[0].y <= median || (*segs[i])[segs[i]->size() - 1].y <= median) {
				one.push_back(segs[i]);
			}
			if ((*segs[i])[0].y >= median || (*segs[i])[segs[i]->size() - 1].y >= median) {
				two.push_back(segs[i]);
			}
		}
	}

	bool again = false;

	// If partitioning didn't make one or both sets smaller (because all segments cross the median),
	// there is no choice but to check it by brute force

	if (one.size() >= segs.size() || two.size() >= segs.size()) {
		for (size_t i = 0; i + 1 < segs.size(); i++) {
			for (size_t j = i + 1; j < segs.size(); j++) {
				if (check_intersections(segs[i], segs[j])) {
					again = true;
				}
			}
		}

		return again;
	}

	// In a more reasonable case, continue partitioning the first half if it is still large,
	// or check all the intersections if it is small.

	if (one.size() > 20) {
		if (partition(one, !direction)) {
			again = true;
		}
	} else {
		for (size_t i = 0; i + 1 < one.size(); i++) {
			for (size_t j = i + 1; j < one.size(); j++) {
				if (check_intersections(one[i], one[j])) {
					again = true;
				}
			}
		}
	}

	// Do the same for the second half

	if (two.size() > 20) {
		if (partition(two, !direction)) {
			again = true;
		}
	} else {
		for (size_t i = 0; i + 1 < two.size(); i++) {
			for (size_t j = i + 1; j < two.size(); j++) {
				if (check_intersections(two[i], two[j])) {
					again = true;
				}
			}
		}
	}

	return again;
}

drawvec scan(drawvec &geom) {
	// Decompose the polygon into segments.

	std::vector<drawvec *> segs;

	size_t n = 0;
	for (size_t i = 0; i < geom.size() - 1; i++) {
		if (geom[i + 1].op == VT_LINETO) {
			if (geom[i].x != geom[i + 1].x || geom[i].y != geom[i + 1].y) {
				drawvec *dv = new drawvec;
				dv->push_back(geom[i]);
				dv->push_back(geom[i + 1]);
				segs.push_back(dv);
				n++;
			}
		}
	}

	// Split the segments by bounding box into smaller subsets
	// until they are reasonably sized or can't be split further.

	while (partition(segs, 0)) {
		// Repeat until no additional changes
	}

	// At this point we have a whole lot of polygon edges and need to reconstruct
	// polygons from them.

	std::vector<drawvec> edges;

	for (size_t i = 0; i < segs.size(); i++) {
		for (size_t j = 0; j + 1 < (*segs[i]).size(); j++) {
			if ((*segs[i])[j].x != (*segs[i])[j + 1].x || (*segs[i])[j].y != (*segs[i])[j + 1].y) {
				drawvec dv;

				dv.push_back((*segs[i])[j]);
				dv.push_back((*segs[i])[j + 1]);

				edges.push_back(dv);
			}
		}
	}

	std::stable_sort(edges.begin(), edges.end(), s_edgecmp);

	// Original segments are no longer needed

	for (size_t i = 0; i < segs.size(); i++) {
		delete segs[i];
		segs[i] = NULL;
	}

	// Index each segment by its starting point

	std::multimap<loc, drawvec *> origins;

	for (size_t i = 0; i < edges.size(); i++) {
		loc l1 = loc(edges[i][0], 1);
		origins.insert(std::pair<struct loc, drawvec *>(l1, &edges[i]));
	}

	// Remove all pairs of edges that are exact opposites, because these are
	// places where two rings with the same polarity are directly adjacent
	// and should be merged, or where an inner ring shares an edge with an
	// outer ring and should be cut away.

	for (size_t i = 0; i < edges.size(); i++) {
		if (edges[i].size() > 0) {
			loc here = loc(edges[i][1], 1);
			std::pair<std::multimap<loc, drawvec *>::iterator, std::multimap<loc, drawvec *>::iterator> options;
			options = origins.equal_range(here);

			for (std::multimap<loc, drawvec *>::iterator it = options.first; it != options.second; ++it) {
				drawvec *option = it->second;

				if (option->size() != 0) {
					if ((*option)[0].x == edges[i][1].x &&
					    (*option)[0].y == edges[i][1].y &&
					    (*option)[1].x == edges[i][0].x &&
					    (*option)[1].y == edges[i][0].y) {
						fprintf(stderr, "removed %lld,%lld %lld,%lld for %lld,%lld %lld,%lld\n",
							(*option)[0].x, (*option)[0].y,
							(*option)[1].x, (*option)[1].y,
							edges[i][0].x, edges[i][0].y,
							edges[i][1].x, edges[i][1].y);
						(*option).clear();
						edges[i].clear();
						break;
					}
				}
			}
		}
	}

	// Use that to reconstruct rings:
	// From each arbitrary starting point, follow the right hand rule to form a ring from it,
	// removing those segments from further consideration. (Marking as used means setting
	// the size of the edge to 0.)

	drawvec ret;

	for (size_t i = 0; i < edges.size(); i++) {
		if (edges[i].size() > 0) {
			drawvec out;

			out.push_back(edges[i][0]);
			out.push_back(edges[i][1]);
			edges[i].clear();

			while (1) {
				struct loc here = loc(out[out.size() - 1], -1);
				double angle = atan2(out[out.size() - 1].y - out[out.size() - 2].y, out[out.size() - 1].x - out[out.size() - 2].x);
				if (angle < 0) {
					angle += 2 * M_PI;
				}

				std::pair<std::multimap<loc, drawvec *>::iterator, std::multimap<loc, drawvec *>::iterator> options;
				options = origins.equal_range(here);

				drawvec *best = NULL;
				double bestangle = 2 * M_PI;
				loc next;

				for (std::multimap<loc, drawvec *>::iterator it = options.first; it != options.second; ++it) {
					drawvec *option = it->second;

					if (option->size() != 0) {
						loc other = loc((*option)[it->first.direction], -1);

						double angle2 = atan2(other.y - out[out.size() - 1].y, other.x - out[out.size() - 1].x);
						if (angle2 < 0) {
							angle2 += 2 * M_PI;
						}

						double diff = angle2 - angle;
						if (diff < 0) {
							diff += 2 * M_PI;
						}
						if (diff > M_PI) {
							diff -= 2 * M_PI;
						}

						if (diff < bestangle) {
							bestangle = diff;
							best = option;
							next = other;
						}

#if 0
						printf("from %lld,%lld %lld,%lld found %lld,%lld to %lld,%lld: %f\n",
							out[out.size() - 2].x, out[out.size() - 2].y, out[out.size() - 1].x, out[out.size() - 1].y,
							(*option)[0].x, (*option)[0].y, (*option)[1].x, (*option)[1].y, diff);
#endif
					}
				}

				if (best == NULL) {
					if (out[0].x != out[out.size() - 1].x || out[0].y != out[out.size() - 1].y) {
						fprintf(stderr, "Polygon is not a loop\n");
					}

					break;
				} else {
					out.push_back(draw(VT_MOVETO, next.x, next.y));
					best->clear();

					// If we've looped, stop instead of trying to add more to the ring
					if (out[0].x == out[out.size() - 1].x && out[0].y == out[out.size() - 1].y) {
						break;
					}
				}
			}

			ret.push_back(draw(VT_MOVETO, out[0].x, out[0].y));
			for (size_t j = 1; j < out.size(); j++) {
				ret.push_back(draw(VT_LINETO, out[j].x, out[j].y));
			}
		}
	}

	// Then afterward, use point-in-polygon tests to figure out which rings are inside
	// which other rings.

	return ret;
}
