#ifndef PROJECTION_HPP
#define PROJECTION_HPP

void lonlat2tile(double lon, double lat, int32_t zoom, int64_t *x, int64_t *y);
void epsg3857totile(double ix, double iy, int32_t zoom, int64_t *x, int64_t *y);
void tile2lonlat(int64_t x, int64_t y, int32_t zoom, double *lon, double *lat);
void tiletoepsg3857(int64_t x, int64_t y, int32_t zoom, double *ox, double *oy);
void set_projection_or_exit(const char *optarg);

struct projection {
	const char *name;
	void (*project)(double ix, double iy, int32_t zoom, int64_t *ox, int64_t *oy);
	void (*unproject)(int64_t ix, int64_t iy, int32_t zoom, double *ox, double *oy);
	const char *alias;
};

extern struct projection *projection;
extern struct projection projections[];

extern uint64_t (*encode_index)(uint32_t wx, uint32_t wy);
extern void (*decode_index)(uint64_t index, uint32_t *wx, uint32_t *wy);

uint64_t encode_quadkey(uint32_t wx, uint32_t wy);
void decode_quadkey(uint64_t index, uint32_t *wx, uint32_t *wy);

uint64_t encode_hilbert(uint32_t wx, uint32_t wy);
void decode_hilbert(uint64_t index, uint32_t *wx, uint32_t *wy);

#endif
