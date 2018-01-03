#ifndef PROJECTION_HPP
#define PROJECTION_HPP

void lonlat2tile(double lon, double lat, int zoom, long *x, long *y);
void epsg3857totile(double ix, double iy, int zoom, long *x, long *y);
void tile2lonlat(long x, long y, int zoom, double *lon, double *lat);
void tiletoepsg3857(long x, long y, int zoom, double *ox, double *oy);
unsigned long encode(unsigned int wx, unsigned int wy);
void decode(unsigned long index, unsigned *wx, unsigned *wy);
void set_projection_or_exit(const char *optarg);

struct projection {
	const char *name;
	void (*project)(double ix, double iy, int zoom, long *ox, long *oy);
	void (*unproject)(long ix, long iy, int zoom, double *ox, double *oy);
	const char *alias;
};

extern struct projection *projection;
extern struct projection projections[];

#endif
