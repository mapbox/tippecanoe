#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include <vector>
#include <atomic>
#include <sqlite3.h>

#define VT_POINT 1
#define VT_LINE 2
#define VT_POLYGON 3

#define VT_END 0
#define VT_MOVETO 1
#define VT_LINETO 2
#define VT_CLOSEPATH 7

// The bitfield is to make sizeof(draw) be 16 instead of 24
// at the cost, apparently, of a 0.7% increase in running time
// for packing and unpacking.
struct draw {
	int64_t x : 40;
	signed char op;
	int64_t y : 40;
	signed char necessary;

	draw(int32_t nop, int64_t nx, int64_t ny)
	    : x(nx),
	      op(nop),
	      y(ny),
	      necessary(0) {
	}

	draw()
	    : x(0),
	      op(0),
	      y(0),
	      necessary(0) {
	}

	bool operator<(draw const &s) const {
		if (y < s.y || (y == s.y && x < s.x)) {
			return true;
		} else {
			return false;
		}
	}

	bool operator==(draw const &s) const {
		return y == s.y && x == s.x;
	}

	bool operator!=(draw const &s) const {
		return y != s.y || x != s.x;
	}
};

typedef std::vector<draw> drawvec;

drawvec decode_geometry(FILE *meta, std::atomic<int64_t> *geompos, int32_t z, uint32_t tx, uint32_t ty, int64_t *bbox, uint32_t initial_x, uint32_t initial_y);
void to_tile_scale(drawvec &geom, int32_t z, int32_t detail);
drawvec remove_noop(drawvec geom, int32_t type, int32_t shift);
drawvec clip_point(drawvec &geom, int32_t z, int64_t buffer);
drawvec clean_or_clip_poly(drawvec &geom, int32_t z, int32_t buffer, bool clip);
drawvec simple_clip_poly(drawvec &geom, int32_t z, int32_t buffer);
drawvec close_poly(drawvec &geom);
drawvec reduce_tiny_poly(drawvec &geom, int32_t z, int32_t detail, bool *reduced, double *accum_area);
drawvec clip_lines(drawvec &geom, int32_t z, int64_t buffer);
drawvec stairstep(drawvec &geom, int32_t z, int32_t detail);
bool point_within_tile(int64_t x, int64_t y, int32_t z);
int32_t quick_check(int64_t *bbox, int32_t z, int64_t buffer);
drawvec simplify_lines(drawvec &geom, int32_t z, int32_t detail, bool mark_tile_bounds, double simplification, size_t retain);
drawvec reorder_lines(drawvec &geom);
drawvec fix_polygon(drawvec &geom);
std::vector<drawvec> chop_polygon(std::vector<drawvec> &geoms);
void check_polygon(drawvec &geom);
double get_area(drawvec &geom, size_t i, size_t j);
double get_mp_area(drawvec &geom);

drawvec simple_clip_poly(drawvec &geom, int64_t x1, int64_t y1, int64_t x2, int64_t y2);
drawvec clip_lines(drawvec &geom, int64_t x1, int64_t y1, int64_t x2, int64_t y2);
drawvec clip_point(drawvec &geom, int64_t x1, int64_t y1, int64_t x2, int64_t y2);

#endif
