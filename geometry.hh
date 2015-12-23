struct draw {
	signed char op;
	long long x;
	long long y;
	int necessary;

	draw(int op, long long x, long long y) {
		this->op = op;
		this->x = x;
		this->y = y;
	}

	draw() {
	}
};

typedef std::vector<draw> drawvec;

drawvec decode_geometry(char **meta, int z, unsigned tx, unsigned ty, int detail, long long *bbox, unsigned initial_x, unsigned initial_y);
void to_tile_scale(drawvec &geom, int z, int detail);
drawvec remove_noop(drawvec geom, int type, int shift);
drawvec clip_point(drawvec &geom, int z, int detail, long long buffer);
drawvec clean_or_clip_poly(drawvec &geom, int z, int detail, int buffer, bool clip);
drawvec close_poly(drawvec &geom);
drawvec reduce_tiny_poly(drawvec &geom, int z, int detail, bool *reduced, double *accum_area);
drawvec clip_lines(drawvec &geom, int z, int detail, long long buffer);
int quick_check(long long *bbox, int z, int detail, long long buffer);
drawvec simplify_lines(drawvec &geom, int z, int detail);
drawvec reorder_lines(drawvec &geom);
drawvec fix_polygon(drawvec &geom);
