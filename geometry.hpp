#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include <vector>
#include <sqlite3.h>

#define VT_POINT 1
#define VT_LINE 2
#define VT_POLYGON 3

#define VT_END ((signed char) 0)
#define VT_MOVETO ((signed char) 1)
#define VT_LINETO ((signed char) 2)
#define VT_CLOSEPATH ((signed char) 7)

// The bitfield is to make sizeof(draw) be 16 instead of 24
// at the cost, apparently, of a 0.7% increase in running time
// for packing and unpacking.
struct draw {
	long x : 40;
	signed char op;
	long y : 40;
	signed char necessary;

	draw(signed char nop, long nx, long ny)
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

drawvec decode_geometry(FILE *meta, off_t *geompos, int z, unsigned tx, unsigned ty, long *bbox, long initial_x, long initial_y);
void to_tile_scale(drawvec &geom, int z, int detail);
drawvec remove_noop(drawvec geom, int type, size_t shift);
drawvec clip_point(drawvec &geom, int z, long buffer);
drawvec clean_or_clip_poly(drawvec &geom, int z, long buffer, bool clip);
drawvec simple_clip_poly(drawvec &geom, int z, long buffer);
drawvec close_poly(drawvec &geom);
drawvec reduce_tiny_poly(drawvec &geom, int z, int detail, bool *reduced, double *accum_area);
drawvec clip_lines(drawvec &geom, int z, long buffer);
drawvec stairstep(drawvec &geom, int z, int detail);
bool point_within_tile(long x, long y, int z);
int quick_check(long *bbox, int z, long buffer);
drawvec simplify_lines(drawvec &geom, int z, int detail, bool mark_tile_bounds, double simplification, size_t retain);
drawvec reverse_lines(drawvec &geom);
drawvec fix_polygon(drawvec &geom);
std::vector<drawvec> chop_polygon(std::vector<drawvec> &geoms);
void check_polygon(drawvec &geom);
double get_area(drawvec &geom, size_t i, size_t j);
double get_mp_area(drawvec &geom);

#endif
