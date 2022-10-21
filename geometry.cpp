#include <iostream>
#include <fstream>
#include <string>
#include <stack>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdio>
#include <unistd.h>
#include <cmath>
#include <limits.h>
#include <sqlite3.h>
#include <mapbox/geometry/point.hpp>
#include <mapbox/geometry/multi_polygon.hpp>
#include <mapbox/geometry/wagyu/wagyu.hpp>
#include <mapbox/geometry/wagyu/quick_clip.hpp>
#include <mapbox/geometry/snap_rounding.hpp>
#include "geometry.hpp"
#include "projection.hpp"
#include "serial.hpp"
#include "main.hpp"
#include "options.hpp"
#include "errors.hpp"

static int clip(double *x0, double *y0, double *x1, double *y1, double xmin, double ymin, double xmax, double ymax);

drawvec decode_geometry(FILE *meta, std::atomic<long long> *geompos, int z, unsigned tx, unsigned ty, long long *bbox, unsigned initial_x, unsigned initial_y) {
	drawvec out;

	bbox[0] = LLONG_MAX;
	bbox[1] = LLONG_MAX;
	bbox[2] = LLONG_MIN;
	bbox[3] = LLONG_MIN;

	long long wx = initial_x, wy = initial_y;

	while (1) {
		draw d;

		if (!deserialize_byte_io(meta, &d.op, geompos)) {
			fprintf(stderr, "Internal error: Unexpected end of file in geometry\n");
			exit(EXIT_IMPOSSIBLE);
		}
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

drawvec from_tile_scale(drawvec const &geom, int z, int detail) {
	drawvec out;
	for (size_t i = 0; i < geom.size(); i++) {
		draw d = geom[i];
		d.x <<= (32 - detail - z);
		d.y <<= (32 - detail - z);
		out.push_back(d);
	}
	return out;
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

double get_area_scaled(const drawvec &geom, size_t i, size_t j) {
	const double max_exact_double = (double) ((1LL << 53) - 1);

	// keep scaling the geometry down until we can calculate its area without overflow
	for (long long scale = 2; scale < (1LL << 30); scale *= 2) {
		long long bx = geom[i].x;
		long long by = geom[i].y;
		bool again = false;

		// https://en.wikipedia.org/wiki/Shoelace_formula
		double area = 0;
		for (size_t k = i; k < j; k++) {
			area += (double) ((geom[k].x - bx) / scale) * (double) ((geom[i + ((k - i + 1) % (j - i))].y - by) / scale);
			if (std::fabs(area) >= max_exact_double) {
				again = true;
				break;
			}
			area -= (double) ((geom[k].y - by) / scale) * (double) ((geom[i + ((k - i + 1) % (j - i))].x - bx) / scale);
			if (std::fabs(area) >= max_exact_double) {
				again = true;
				break;
			}
		}

		if (again) {
			continue;
		} else {
			area /= 2;
			return area * scale * scale;
		}
	}

	fprintf(stderr, "get_area_scaled: can't happen\n");
	exit(EXIT_IMPOSSIBLE);
}

double get_area(const drawvec &geom, size_t i, size_t j) {
	const double max_exact_double = (double) ((1LL << 53) - 1);

	// Coordinates in `geom` are 40-bit integers, so there is no good way
	// to multiply them without possible precision loss. Since they probably
	// do not use the full precision, shift them nearer to the origin so
	// their product is more likely to be exactly representable as a double.
	//
	// (In practice they are actually 34-bit integers: 32 bits for the
	// Mercator world plane, plus another two bits so features can stick
	// off either the left or right side. But that is still too many bits
	// for the product to fit either in a 64-bit long long or in a
	// double where the largest exact integer is 2^53.)
	//
	// If the intermediate calculation still exceeds 2^53, start trying to
	// recalculate the area by scaling down the geometry. This will not
	// produce as precise an area, but it will still be close, and the
	// sign will be correct, which is more important, since the sign
	// determines the winding order of the rings. We can then use that
	// sign with this generally more precise area calculation.

	long long bx = geom[i].x;
	long long by = geom[i].y;

	// https://en.wikipedia.org/wiki/Shoelace_formula
	double area = 0;
	bool overflow = false;
	for (size_t k = i; k < j; k++) {
		area += (double) (geom[k].x - bx) * (double) (geom[i + ((k - i + 1) % (j - i))].y - by);
		if (std::fabs(area) >= max_exact_double) {
			overflow = true;
		}
		area -= (double) (geom[k].y - by) * (double) (geom[i + ((k - i + 1) % (j - i))].x - bx);
		if (std::fabs(area) >= max_exact_double) {
			overflow = true;
		}
	}
	area /= 2;

	if (overflow) {
		double scaled_area = get_area_scaled(geom, i, j);
		if ((area < 0 && scaled_area > 0) || (area > 0 && scaled_area < 0)) {
			area = -area;
		}
	}

	return area;
}

double get_mp_area(drawvec &geom) {
	double ret = 0;

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			size_t j;

			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op != VT_LINETO) {
					break;
				}
			}

			ret += get_area(geom, i, j);
			i = j - 1;
		}
	}

	return ret;
}

static void decode_clipped(mapbox::geometry::multi_polygon<long long> &t, drawvec &out) {
	out.clear();

	for (size_t i = 0; i < t.size(); i++) {
		for (size_t j = 0; j < t[i].size(); j++) {
			drawvec ring;

			for (size_t k = 0; k < t[i][j].size(); k++) {
				ring.push_back(draw((k == 0) ? VT_MOVETO : VT_LINETO, t[i][j][k].x, t[i][j][k].y));
			}

			if (ring.size() > 0 && ring[ring.size() - 1] != ring[0]) {
				fprintf(stderr, "Had to close ring\n");
				ring.push_back(draw(VT_LINETO, ring[0].x, ring[0].y));
			}

			double area = get_area(ring, 0, ring.size());

			if ((j == 0 && area < 0) || (j != 0 && area > 0)) {
				fprintf(stderr, "Ring area has wrong sign: %f for %zu\n", area, j);
				exit(EXIT_IMPOSSIBLE);
			}

			for (size_t k = 0; k < ring.size(); k++) {
				out.push_back(ring[k]);
			}
		}
	}
}

drawvec clean_or_clip_poly(drawvec &geom, int z, int buffer, bool clip) {
	mapbox::geometry::wagyu::wagyu<long long> wagyu;

	geom = remove_noop(geom, VT_POLYGON, 0);
	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			size_t j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op != VT_LINETO) {
					break;
				}
			}

			if (j >= i + 4) {
				mapbox::geometry::linear_ring<long long> lr;

				for (size_t k = i; k < j; k++) {
					lr.push_back(mapbox::geometry::point<long long>(geom[k].x, geom[k].y));
				}

				if (lr.size() >= 3) {
					wagyu.add_ring(lr);
				}
			}

			i = j - 1;
		}
	}

	if (clip) {
		long long area = 0xFFFFFFFF;
		if (z != 0) {
			area = 1LL << (32 - z);
		}
		long long clip_buffer = buffer * area / 256;

		mapbox::geometry::linear_ring<long long> lr;

		lr.push_back(mapbox::geometry::point<long long>(-clip_buffer, -clip_buffer));
		lr.push_back(mapbox::geometry::point<long long>(-clip_buffer, area + clip_buffer));
		lr.push_back(mapbox::geometry::point<long long>(area + clip_buffer, area + clip_buffer));
		lr.push_back(mapbox::geometry::point<long long>(area + clip_buffer, -clip_buffer));
		lr.push_back(mapbox::geometry::point<long long>(-clip_buffer, -clip_buffer));

		wagyu.add_ring(lr, mapbox::geometry::wagyu::polygon_type_clip);
	}

	mapbox::geometry::multi_polygon<long long> result;
	try {
		wagyu.execute(mapbox::geometry::wagyu::clip_type_union, result, mapbox::geometry::wagyu::fill_type_positive, mapbox::geometry::wagyu::fill_type_positive);
	} catch (std::runtime_error &e) {
		FILE *f = fopen("/tmp/wagyu.log", "w");
		fprintf(f, "%s\n", e.what());
		fprintf(stderr, "%s\n", e.what());
		fprintf(f, "[");

		for (size_t i = 0; i < geom.size(); i++) {
			if (geom[i].op == VT_MOVETO) {
				size_t j;
				for (j = i + 1; j < geom.size(); j++) {
					if (geom[j].op != VT_LINETO) {
						break;
					}
				}

				if (j >= i + 4) {
					mapbox::geometry::linear_ring<long long> lr;

					if (i != 0) {
						fprintf(f, ",");
					}
					fprintf(f, "[");

					for (size_t k = i; k < j; k++) {
						lr.push_back(mapbox::geometry::point<long long>(geom[k].x, geom[k].y));
						if (k != i) {
							fprintf(f, ",");
						}
						fprintf(f, "[%lld,%lld]", geom[k].x, geom[k].y);
					}

					fprintf(f, "]");

					if (lr.size() >= 3) {
					}
				}

				i = j - 1;
			}
		}

		fprintf(f, "]");
		fprintf(f, "\n\n\n\n\n");

		fclose(f);
		fprintf(stderr, "Internal error: Polygon cleaning failed. Log in /tmp/wagyu.log\n");
		exit(EXIT_IMPOSSIBLE);
	}

	drawvec ret;
	decode_clipped(result, ret);
	return ret;
}

/* pnpoly:
Copyright (c) 1970-2003, Wm. Randolph Franklin

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimers.
Redistributions in binary form must reproduce the above copyright notice in the documentation and/or other materials provided with the distribution.
The name of W. Randolph Franklin may not be used to endorse or promote products derived from this Software without specific prior written permission.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

int pnpoly(const drawvec &vert, size_t start, size_t nvert, long long testx, long long testy) {
	size_t i, j;
	bool c = false;
	for (i = 0, j = nvert - 1; i < nvert; j = i++) {
		if (((vert[i + start].y > testy) != (vert[j + start].y > testy)) &&
		    (testx < (vert[j + start].x - vert[i + start].x) * (testy - vert[i + start].y) / (double) (vert[j + start].y - vert[i + start].y) + vert[i + start].x))
			c = !c;
	}
	return c;
}

void check_polygon(drawvec &geom) {
	geom = remove_noop(geom, VT_POLYGON, 0);

	mapbox::geometry::multi_polygon<long long> mp;
	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			size_t j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op != VT_LINETO) {
					break;
				}
			}

			if (j >= i + 4) {
				mapbox::geometry::linear_ring<long long> lr;

				for (size_t k = i; k < j; k++) {
					lr.push_back(mapbox::geometry::point<long long>(geom[k].x, geom[k].y));
				}

				if (lr.size() >= 3) {
					mapbox::geometry::polygon<long long> p;
					p.push_back(lr);
					mp.push_back(p);
				}
			}

			i = j - 1;
		}
	}

	mapbox::geometry::multi_polygon<long long> mp2 = mapbox::geometry::snap_round(mp, true, true);
	if (mp != mp2) {
		fprintf(stderr, "Internal error: self-intersecting polygon\n");
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

drawvec simple_clip_poly(drawvec &geom, long long minx, long long miny, long long maxx, long long maxy) {
	drawvec out;

	mapbox::geometry::point<long long> min(minx, miny);
	mapbox::geometry::point<long long> max(maxx, maxy);
	mapbox::geometry::box<long long> bbox(min, max);

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			size_t j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op != VT_LINETO) {
					break;
				}
			}

			mapbox::geometry::linear_ring<long long> ring;
			for (size_t k = i; k < j; k++) {
				ring.push_back(mapbox::geometry::point<long long>(geom[k].x, geom[k].y));
			}

			mapbox::geometry::linear_ring<long long> lr = mapbox::geometry::wagyu::quick_clip::quick_lr_clip(ring, bbox);

			if (lr.size() > 0) {
				for (size_t k = 0; k < lr.size(); k++) {
					if (k == 0) {
						out.push_back(draw(VT_MOVETO, lr[k].x, lr[k].y));
					} else {
						out.push_back(draw(VT_LINETO, lr[k].x, lr[k].y));
					}
				}

				if (lr.size() > 0 && lr[0] != lr[lr.size() - 1]) {
					out.push_back(draw(VT_LINETO, lr[0].x, lr[0].y));
				}
			}

			i = j - 1;
		} else {
			fprintf(stderr, "Unexpected operation in polygon %d\n", (int) geom[i].op);
			exit(EXIT_IMPOSSIBLE);
		}
	}

	return out;
}

drawvec simple_clip_poly(drawvec &geom, int z, int buffer) {
	long long area = 1LL << (32 - z);
	long long clip_buffer = buffer * area / 256;

	return simple_clip_poly(geom, -clip_buffer, -clip_buffer, area + clip_buffer, area + clip_buffer);
}

drawvec reduce_tiny_poly(drawvec &geom, int z, int detail, bool *reduced, double *accum_area, serial_feature *this_feature, serial_feature *tiny_feature) {
	drawvec out;
	const double pixel = (1LL << (32 - detail - z)) * (double) tiny_polygon_size;
	bool includes_real = false;
	bool includes_dust = false;

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

				// i.e., this ring (inner or outer) is small enough that we are including it
				// in a tiny polygon rather than letting it represent itself,
				// OR it is an inner ring and we haven't output an outer ring for it to be
				// cut out of, so we are just subtracting its area from the tiny polygon
				// rather than trying to deal with it geometrically
				if (std::fabs(area) <= pixel * pixel || (area < 0 && !included_last_outer)) {
					// printf("area is only %f vs %lld so using square\n", area, pixel * pixel);

					*accum_area += area;
					if (area > 0 && *accum_area > pixel * pixel) {
						// XXX use centroid;

						out.push_back(draw(VT_MOVETO, geom[i].x - pixel / 2, geom[i].y - pixel / 2));
						out.push_back(draw(VT_LINETO, geom[i].x - pixel / 2 + pixel, geom[i].y - pixel / 2));
						out.push_back(draw(VT_LINETO, geom[i].x - pixel / 2 + pixel, geom[i].y - pixel / 2 + pixel));
						out.push_back(draw(VT_LINETO, geom[i].x - pixel / 2, geom[i].y - pixel / 2 + pixel));
						out.push_back(draw(VT_LINETO, geom[i].x - pixel / 2, geom[i].y - pixel / 2));
						includes_dust = true;

						*accum_area -= pixel * pixel;
					}

					if (area > 0) {
						included_last_outer = false;
					}
				}
				// i.e., this ring is large enough that it gets to represent itself
				else {
					// printf("area is %f so keeping instead of %lld\n", area, pixel * pixel);

					for (size_t k = i; k <= j && k < geom.size(); k++) {
						out.push_back(geom[k]);
					}

					// which means that the overall polygon has a real geometry,
					// which means that it gets to be simplified.
					*reduced = false;
					includes_real = true;

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
			includes_real = true;
		}
	}

	if (!includes_real) {
		if (includes_dust) {
			// this geometry is just dust, so if there is another feature that
			// contributed to the dust that is larger than this feature,
			// keep its attributes instead of this one that just happened to be
			// the one that hit the threshold of survival.

			if (tiny_feature->extent > this_feature->extent) {
				*this_feature = *tiny_feature;
				tiny_feature->extent = 0;
			}
		} else {
			// this is a feature that we are throwing away, so hang on to it
			// attributes if it is bigger than the biggest one we threw away so far

			if (this_feature->extent > tiny_feature->extent) {
				*tiny_feature = *this_feature;
			}
		}
	}

	return out;
}

drawvec clip_point(drawvec &geom, int z, long long buffer) {
	long long min = 0;
	long long area = 1LL << (32 - z);

	min -= buffer * area / 256;
	area += buffer * area / 256;

	return clip_point(geom, min, min, area, area);
}

drawvec clip_point(drawvec &geom, long long minx, long long miny, long long maxx, long long maxy) {
	drawvec out;

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].x >= minx && geom[i].y >= miny && geom[i].x <= maxx && geom[i].y <= maxy) {
			out.push_back(geom[i]);
		}
	}

	return out;
}

int quick_check(long long *bbox, int z, long long buffer) {
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

bool point_within_tile(long long x, long long y, int z) {
	// No adjustment for buffer, because the point must be
	// strictly within the tile to appear exactly once

	long long area = 1LL << (32 - z);

	return x >= 0 && y >= 0 && x < area && y < area;
}

drawvec clip_lines(drawvec &geom, int z, long long buffer) {
	long long min = 0;
	long long area = 1LL << (32 - z);
	min -= buffer * area / 256;
	area += buffer * area / 256;

	return clip_lines(geom, min, min, area, area);
}

drawvec clip_lines(drawvec &geom, long long minx, long long miny, long long maxx, long long maxy) {
	drawvec out;

	for (size_t i = 0; i < geom.size(); i++) {
		if (i > 0 && (geom[i - 1].op == VT_MOVETO || geom[i - 1].op == VT_LINETO) && geom[i].op == VT_LINETO) {
			double x1 = geom[i - 1].x;
			double y1 = geom[i - 1].y;

			double x2 = geom[i - 0].x;
			double y2 = geom[i - 0].y;

			int c = clip(&x1, &y1, &x2, &y2, minx, miny, maxx, maxy);

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
static void douglas_peucker(drawvec &geom, int start, int n, double e, size_t kept, size_t retain) {
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

			if ((distance > e || kept < retain) && distance > max_distance) {
				farthest_element_index = i;
				max_distance = distance;
			}
		}

		if (max_distance >= 0) {
			// mark idx as necessary
			geom[start + farthest_element_index].necessary = 1;
			kept++;

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

drawvec simplify_lines(drawvec &geom, int z, int detail, bool mark_tile_bounds, double simplification, size_t retain, drawvec const &shared_nodes) {
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

		if (prevent[P_SIMPLIFY_SHARED_NODES]) {
			auto pt = std::lower_bound(shared_nodes.begin(), shared_nodes.end(), geom[i]);
			if (pt != shared_nodes.end() && *pt == geom[i]) {
				geom[i].necessary = true;
			}
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

			// empirical mapping from douglas-peucker simplifications
			// to visvalingam simplifications that yield similar
			// output sizes
			double sim = simplification * (0.1596 * z + 0.878);
			double scale = (res * sim) * (res * sim);
			scale = exp(1.002 * log(scale) + 0.3043);

			if (j - i > 1) {
				if (additional[A_VISVALINGAM]) {
					visvalingam(geom, i, j, scale, retain);
				} else {
					douglas_peucker(geom, i, j - i, res * simplification, 2, retain);
				}
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

	unsigned long long l1 = encode_index(geom[0].x, geom[0].y);
	unsigned long long l2 = encode_index(geom[geom.size() - 1].x, geom[geom.size() - 1].y);

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

			bool reverse_ring = false;
			if (prevent[P_USE_SOURCE_POLYGON_WINDING]) {
				// GeoJSON winding is reversed from vector winding
				reverse_ring = true;
			} else if (prevent[P_REVERSE_SOURCE_POLYGON_WINDING]) {
				// GeoJSON winding is reversed from vector winding
				reverse_ring = false;
			} else {
				double area = get_area(ring, 0, ring.size());
				if ((area > 0) != outer) {
					reverse_ring = true;
				}
			}

			if (reverse_ring) {
				drawvec tmp;
				for (int a = ring.size() - 1; a >= 0; a--) {
					tmp.push_back(ring[a]);
				}
				ring = tmp;
			}

			// calculate centroid
			// a + 1 < size() because point 0 is duplicated at the end
			long long xtotal = 0;
			long long ytotal = 0;
			long long count = 0;
			for (size_t a = 0; a + 1 < ring.size(); a++) {
				xtotal += ring[a].x;
				ytotal += ring[a].y;
				count++;
			}
			xtotal /= count;
			ytotal /= count;

			// figure out which point is furthest from the centroid
			long long dist2 = 0;
			long long furthest = 0;
			for (size_t a = 0; a + 1 < ring.size(); a++) {
				long long xd = ring[a].x - xtotal;
				long long yd = ring[a].y - ytotal;
				long long d2 = xd * xd + yd * yd;
				if (d2 > dist2) {
					dist2 = d2;
					furthest = a;
				}
			}

			// then figure out which point is furthest from *that*
			long long dist2b = 0;
			long long furthestb = 0;
			for (size_t a = 0; a + 1 < ring.size(); a++) {
				long long xd = ring[a].x - ring[furthest].x;
				long long yd = ring[a].y - ring[furthest].y;
				long long d2 = xd * xd + yd * yd;
				if (d2 > dist2b) {
					dist2b = d2;
					furthestb = a;
				}
			}

			// rotate ring so the furthest point is the duplicated one.
			// the idea is that simplification will then be more efficient,
			// never wasting the start and end points, which are always retained,
			// on a point that has little impact on the shape.

			// Copy ring into output, fixing the moveto/lineto ops if necessary because of
			// reversal or closing

			for (size_t a = 0; a < ring.size(); a++) {
				size_t a2 = (a + furthestb) % (ring.size() - 1);

				if (a == 0) {
					out.push_back(draw(VT_MOVETO, ring[a2].x, ring[a2].y));
				} else {
					out.push_back(draw(VT_LINETO, ring[a2].x, ring[a2].y));
				}
			}

			// Next ring or polygon begins on the non-lineto that ended this one
			// and is not an outer ring unless there is a terminator first

			i = j - 1;
			outer = 0;
		} else {
			fprintf(stderr, "Internal error: polygon ring begins with %d, not moveto\n", geom[i].op);
			exit(EXIT_IMPOSSIBLE);
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
			if (outcodeOut & TOP) {	 // point is above the clip rectangle
				x = *x0 + (*x1 - *x0) * (ymax - *y0) / (*y1 - *y0);
				y = ymax;
			} else if (outcodeOut & BOTTOM) {  // point is below the clip rectangle
				x = *x0 + (*x1 - *x0) * (ymin - *y0) / (*y1 - *y0);
				y = ymin;
			} else if (outcodeOut & RIGHT) {  // point is to the right of clip rectangle
				y = *y0 + (*y1 - *y0) * (xmax - *x0) / (*x1 - *x0);
				x = xmax;
			} else if (outcodeOut & LEFT) {	 // point is to the left of clip rectangle
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

drawvec stairstep(drawvec &geom, int z, int detail) {
	drawvec out;
	double scale = 1 << (32 - detail - z);

	for (size_t i = 0; i < geom.size(); i++) {
		geom[i].x = std::floor(geom[i].x / scale);
		geom[i].y = std::floor(geom[i].y / scale);
	}

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			out.push_back(geom[i]);
		} else if (out.size() > 0) {
			long long x0 = out[out.size() - 1].x;
			long long y0 = out[out.size() - 1].y;
			long long x1 = geom[i].x;
			long long y1 = geom[i].y;
			bool swap = false;

			if (y0 < y1) {
				swap = true;
				std::swap(x0, x1);
				std::swap(y0, y1);
			}

			long long xx = x0, yy = y0;
			long long dx = std::abs(x1 - x0);
			long long sx = (x0 < x1) ? 1 : -1;
			long long dy = std::abs(y1 - y0);
			long long sy = (y0 < y1) ? 1 : -1;
			long long err = ((dx > dy) ? dx : -dy) / 2;
			int last = -1;

			drawvec tmp;
			tmp.push_back(draw(VT_LINETO, xx, yy));

			while (xx != x1 || yy != y1) {
				long long e2 = err;

				if (e2 > -dx) {
					err -= dy;
					xx += sx;
					if (last == 1) {
						tmp[tmp.size() - 1] = draw(VT_LINETO, xx, yy);
					} else {
						tmp.push_back(draw(VT_LINETO, xx, yy));
					}
					last = 1;
				}
				if (e2 < dy) {
					err += dx;
					yy += sy;
					if (last == 2) {
						tmp[tmp.size() - 1] = draw(VT_LINETO, xx, yy);
					} else {
						tmp.push_back(draw(VT_LINETO, xx, yy));
					}
					last = 2;
				}
			}

			if (swap) {
				for (size_t j = tmp.size(); j > 0; j--) {
					out.push_back(tmp[j - 1]);
				}
			} else {
				for (size_t j = 0; j < tmp.size(); j++) {
					out.push_back(tmp[j]);
				}
			}

			// out.push_back(draw(VT_LINETO, xx, yy));
		} else {
			fprintf(stderr, "Can't happen: stairstepping lineto with no moveto\n");
			exit(EXIT_IMPOSSIBLE);
		}
	}

	for (size_t i = 0; i < out.size(); i++) {
		out[i].x *= 1 << (32 - detail - z);
		out[i].y *= 1 << (32 - detail - z);
	}

	return out;
}

// https://github.com/Turfjs/turf/blob/master/packages/turf-center-of-mass/index.ts
//
// The MIT License (MIT)
//
// Copyright (c) 2019 Morgan Herlocker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
draw centerOfMass(const drawvec &dv, size_t start, size_t end, draw centre) {
	std::vector<draw> coords;
	for (size_t i = start; i < end; i++) {
		coords.push_back(dv[i]);
	}

	// First, we neutralize the feature (set it around coordinates [0,0]) to prevent rounding errors
	// We take any point to translate all the points around 0
	draw translation = centre;
	double sx = 0;
	double sy = 0;
	double sArea = 0;
	draw pi, pj;
	double xi, xj, yi, yj, a;

	std::vector<draw> neutralizedPoints;
	for (size_t i = 0; i < coords.size(); i++) {
		neutralizedPoints.push_back(draw(coords[i].op, coords[i].x - translation.x, coords[i].y - translation.y));
	}

	for (size_t i = 0; i < coords.size() - 1; i++) {
		// pi is the current point
		pi = neutralizedPoints[i];
		xi = pi.x;
		yi = pi.y;

		// pj is the next point (pi+1)
		pj = neutralizedPoints[i + 1];
		xj = pj.x;
		yj = pj.y;

		// a is the common factor to compute the signed area and the final coordinates
		a = xi * yj - xj * yi;

		// sArea is the sum used to compute the signed area
		sArea += a;

		// sx and sy are the sums used to compute the final coordinates
		sx += (xi + xj) * a;
		sy += (yi + yj) * a;
	}

	// Shape has no area: fallback on turf.centroid
	if (sArea == 0) {
		return centre;
	} else {
		// Compute the signed area, and factorize 1/6A
		double area = sArea * 0.5;
		double areaFactor = 1 / (6 * area);

		// Compute the final coordinates, adding back the values that have been neutralized
		return draw(VT_MOVETO, translation.x + areaFactor * sx, translation.y + areaFactor * sy);
	}
}

double label_goodness(const drawvec &dv, size_t start, size_t count, long long x, long long y) {
	if (!pnpoly(dv, start, count, x, y)) {
		return 0;  // outside the polygon is as bad as it gets
	}

	double closest = INFINITY;  // square of closest distance to the border

	for (size_t i = start; i < start + count; i++) {
		double dx = dv[i].x - x;
		double dy = dv[i].y - y;
		double squared = dx * dx + dy * dy;
		if (squared < closest) {
			closest = squared;
		}
	}

	return sqrt(closest);
}

// Generate a label point for a polygon feature.
//
// A good label point will be near the center of the feature and far from any border.
//
// Polylabel is supposed to be able to do this optimally, but can be quite slow
// and sometimes still produces some odd results.
//
// The centroid is often off-center because edges with many curves will be
// weighted higher than edges with straight lines.
//
// Turf's center-of-mass algorithm generally does a good job, but can sometimes
// find a point that is outside the bounds of the polygon or quite close to the edge.
//
// So prefer the center of mass, but if it produces something too close to the border
// or outside the polygon, try a series of gridded points within the feature's bounding box
// until something works well, or if nothing does after several iterations, use the
// least-bad option.

drawvec polygon_to_anchor(const drawvec &geom) {
	size_t start = 0, end = 0;
	size_t best_area = 0;

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO) {
			size_t j;
			for (j = i + 1; j < geom.size(); j++) {
				if (geom[j].op != VT_LINETO) {
					break;
				}
			}

			double area = get_area(geom, i, j);
			if (area > best_area) {
				start = i;
				end = j;
				best_area = area;
			}

			i = j - 1;
		}
	}

	if (best_area > 0) {
		long long xsum = 0;
		long long ysum = 0;
		size_t count = 0;
		long long xmin = LLONG_MAX, ymin = LLONG_MAX, xmax = LLONG_MIN, ymax = LLONG_MIN;

		// Calculate centroid and bounding box.
		// start + 1 to exclude the first point, which is duplicated as the last
		for (size_t k = start + 1; k < end; k++) {
			xsum += geom[k].x;
			ysum += geom[k].y;
			count++;

			xmin = std::min(xmin, geom[k].x);
			ymin = std::min(ymin, geom[k].y);
			xmax = std::max(xmax, geom[k].x);
			ymax = std::max(ymax, geom[k].y);
		}

		if (count > 0) {
			draw centroid(VT_MOVETO, xsum / count, ysum / count);
			draw d = centerOfMass(geom, start, end, centroid);

			double radius = sqrt(best_area / M_PI);
			double goodness_threshold = radius / 5;

			double goodness = label_goodness(geom, start, end - start - 1, d.x, d.y);
			if (goodness < goodness_threshold) {
				// Label is too close to the border or outside it,
				// so try some other possible points

				for (long long sub = 2;
				     sub < 32 && (xmax - xmin) > 2 * sub && (ymax - ymin) > 2 * sub;
				     sub *= 2) {
					for (long long x = 1; x < sub; x++) {
						for (long long y = 1; y < sub; y++) {
							draw maybe(VT_MOVETO,
								   xmin + x * (xmax - xmin) / sub,
								   ymin + y * (ymax - ymin) / sub);

							double maybe_goodness = label_goodness(geom, start, end - start, maybe.x, maybe.y);
							if (maybe_goodness > goodness) {
								// better than the previous
								d = maybe;
								goodness = maybe_goodness;
							}
						}
					}

					if (goodness > goodness_threshold) {
						break;
					}
				}

				// There is nothing really good. Is the centroid maybe better?
				// If not, we're stuck with whatever the best we found was.
				if (label_goodness(geom, start, end - start, centroid.x, centroid.y) > goodness) {
					d = centroid;
				}
			}

			drawvec dv;
			dv.push_back(d);
			return dv;
		}
	}

	return drawvec();
}


static double dist(long long x1, long long y1, long long x2, long long y2) {
	double dx = x2 - x1;
	double dy = y2 - y1;
	return sqrt(dx * dx + dy * dy);
}

drawvec spiral_anchors(drawvec const &geom, int tx, int ty, int z, unsigned long long label_point) {
	drawvec out;

	// anchor point in world coordinates
	unsigned wx, wy;
	decode_index(label_point, &wx, &wy);

	// upper left of tile in world coordinates
	long long tx1 = 0, ty1 = 0;
	// lower right of tile in world coordinates;
	long long tx2 = 1LL << 32, ty2 = 1LL << 32;
	if (z != 0) {
		tx1 = (long long) tx << (32 - z);
		ty1 = (long long) ty << (32 - z);

		tx2 = (long long) (tx + 1) << (32 - z);
		ty2 = (long long) (ty + 1) << (32 - z);
	}

	// min and max distance of corners of this tile from the label point
	double min_radius, max_radius;
	if (wx >= tx1 && wx <= tx2 && wy >= ty1 && wy <= ty2) {
		// center is in this tile,
		// so min is 0, max can't be more than sqrt(2) tiles
		min_radius = 0;
		max_radius = sqrt(2);
	} else {
		// center is elsewhere,
		// so min is at most sqrt(2)/2 tiles short of the center of the tile
		// and max is at most sqrt(2)/2 tiles past the center of the tile
		min_radius = std::max(0.0, dist(wx, wy, (tx1 + tx2) / 2, (ty1 + ty2) / 2) / (1LL << (32 - z)) - sqrt(2) / 2);
		max_radius = min_radius + sqrt(2);
	}

	// labels complete a circle every 0.4 tiles of radius
	const double spiral_dist = 0.4;

	size_t min_i = pow(min_radius / spiral_dist, 2);
	size_t max_i = pow(max_radius / spiral_dist, 2);

	// https://craftofcoding.wordpress.com/tag/sunflower-spiral/
	double angle = M_PI * (3.0 - sqrt(5.0));  // 137.5 in radians

	for (size_t i = min_i; i <= max_i; i++) {
		double r = sqrt(i) * spiral_dist;
		double theta = i * angle;
		// Convert polar to cartesian
		long long x = r * cos(theta) * (1LL << (32 - z)) + wx;
		long long y = r * sin(theta) * (1LL << (32 - z)) + wy;

		if (x >= tx1 && x <= tx2 && y >= ty1 && y <= ty2) {
			// is it actually inside the bounding box of the feature? then keep it.

			for (size_t a = 0; a < geom.size(); a++) {
				if (geom[a].op == VT_MOVETO) {
					size_t b;
					for (b = a + 1; b < geom.size(); b++) {
						if (geom[b].op != VT_LINETO) {
							break;
						}
					}

					// If it's the central label, it's the best we've got,
					// so accept it in any case. If it's from the outer spiral,
					// don't use it if it's too close to a border.
					if (i == 0) {
						out.push_back(draw(VT_MOVETO, x - tx1, y - ty1));
						break;
					} else {
						double tilesize = 1LL << (32 - z);
						double goodness_threshold = tilesize / 100;
						if (label_goodness(geom, a, b - a, x - tx1, y - ty1) > goodness_threshold) {
							out.push_back(draw(VT_MOVETO, x - tx1, y - ty1));
							break;
						}
					}

					a = b - 1;
				}
			}
		}
	}

	return out;
}
