#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <atomic>
#include "projection.hpp"

uint64_t (*encode_index)(uint32_t wx, uint32_t wy) = NULL;
void (*decode_index)(uint64_t index, uint32_t *wx, uint32_t *wy) = NULL;

struct projection projections[] = {
	{"EPSG:4326", lonlat2tile, tile2lonlat, "urn:ogc:def:crs:OGC:1.3:CRS84"},
	{"EPSG:3857", epsg3857totile, tiletoepsg3857, "urn:ogc:def:crs:EPSG::3857"},
	{NULL, NULL, NULL, NULL},
};

struct projection *projection = &projections[0];

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
void lonlat2tile(double lon, double lat, int32_t zoom, int64_t *x, int64_t *y) {
	// Place infinite and NaN coordinates off the edge of the Mercator plane

	int32_t lat_class = fpclassify(lat);
	int32_t lon_class = fpclassify(lon);

	if (lat_class == FP_INFINITE || lat_class == FP_NAN) {
		lat = 89.9;
	}
	if (lon_class == FP_INFINITE || lon_class == FP_NAN) {
		lon = 360;
	}

	// Must limit latitude somewhere to prevent overflow.
	// 89.9 degrees latitude is 0.621 worlds beyond the edge of the flat earth,
	// hopefully far enough out that there are few expectations about the shape.
	if (lat < -89.9) {
		lat = -89.9;
	}
	if (lat > 89.9) {
		lat = 89.9;
	}

	if (lon < -360) {
		lon = -360;
	}
	if (lon > 360) {
		lon = 360;
	}

	double lat_rad = lat * M_PI / 180;
	uint64_t n = 1LL << zoom;

	int64_t llx = n * ((lon + 180) / 360);
	int64_t lly = n * (1 - (log(tan(lat_rad) + 1 / cos(lat_rad)) / M_PI)) / 2;

	*x = llx;
	*y = lly;
}

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
void tile2lonlat(int64_t x, int64_t y, int32_t zoom, double *lon, double *lat) {
	uint64_t n = 1LL << zoom;
	*lon = 360.0 * x / n - 180.0;
	*lat = atan(sinh(M_PI * (1 - 2.0 * y / n))) * 180.0 / M_PI;
}

void epsg3857totile(double ix, double iy, int32_t zoom, int64_t *x, int64_t *y) {
	// Place infinite and NaN coordinates off the edge of the Mercator plane

	int32_t iy_class = fpclassify(iy);
	int32_t ix_class = fpclassify(ix);

	if (iy_class == FP_INFINITE || iy_class == FP_NAN) {
		iy = 40000000.0;
	}
	if (ix_class == FP_INFINITE || ix_class == FP_NAN) {
		ix = 40000000.0;
	}

	*x = ix * (1LL << 31) / 6378137.0 / M_PI + (1LL << 31);
	*y = ((1LL << 32) - 1) - (iy * (1LL << 31) / 6378137.0 / M_PI + (1LL << 31));

	if (zoom != 0) {
		*x >>= (32 - zoom);
		*y >>= (32 - zoom);
	}
}

void tiletoepsg3857(int64_t ix, int64_t iy, int32_t zoom, double *ox, double *oy) {
	if (zoom != 0) {
		ix <<= (32 - zoom);
		iy <<= (32 - zoom);
	}

	*ox = (ix - (1LL << 31)) * M_PI * 6378137.0 / (1LL << 31);
	*oy = ((1LL << 32) - 1 - iy - (1LL << 31)) * M_PI * 6378137.0 / (1LL << 31);
}

// https://en.wikipedia.org/wiki/Hilbert_curve

void hilbert_rot(uint64_t n, uint32_t *x, uint32_t *y, uint64_t rx, uint64_t ry) {
	if (ry == 0) {
		if (rx == 1) {
			*x = n - 1 - *x;
			*y = n - 1 - *y;
		}

		uint32_t t = *x;
		*x = *y;
		*y = t;
	}
}

uint64_t hilbert_xy2d(uint64_t n, uint32_t x, uint32_t y) {
	uint64_t d = 0;
	uint64_t rx, ry;

	for (uint64_t s = n / 2; s > 0; s /= 2) {
		rx = (x & s) != 0;
		ry = (y & s) != 0;

		d += s * s * ((3 * rx) ^ ry);
		hilbert_rot(s, &x, &y, rx, ry);
	}

	return d;
}

void hilbert_d2xy(uint64_t n, uint64_t d, uint32_t *x, uint32_t *y) {
	uint64_t rx, ry;
	uint64_t t = d;

	*x = *y = 0;
	for (uint64_t s = 1; s < n; s *= 2) {
		rx = 1 & (t / 2);
		ry = 1 & (t ^ rx);
		hilbert_rot(s, x, y, rx, ry);
		*x += s * rx;
		*y += s * ry;
		t /= 4;
	}
}

uint64_t encode_hilbert(uint32_t wx, uint32_t wy) {
	return hilbert_xy2d(1LL << 32, wx, wy);
}

void decode_hilbert(uint64_t index, uint32_t *wx, uint32_t *wy) {
	hilbert_d2xy(1LL << 32, index, wx, wy);
}

uint64_t encode_quadkey(uint32_t wx, uint32_t wy) {
	uint64_t out = 0;

	int32_t i;
	for (i = 0; i < 32; i++) {
		uint64_t v = ((wx >> (32 - (i + 1))) & 1) << 1;
		v |= (wy >> (32 - (i + 1))) & 1;
		v = v << (64 - 2 * (i + 1));

		out |= v;
	}

	return out;
}

static std::atomic<unsigned char> decodex[256];
static std::atomic<unsigned char> decodey[256];

void decode_quadkey(uint64_t index, uint32_t *wx, uint32_t *wy) {
	static std::atomic<int32_t> initialized(0);
	if (!initialized) {
		for (size_t ix = 0; ix < 256; ix++) {
			size_t xx = 0, yy = 0;

			for (size_t i = 0; i < 32; i++) {
				xx |= ((ix >> (64 - 2 * (i + 1) + 1)) & 1) << (32 - (i + 1));
				yy |= ((ix >> (64 - 2 * (i + 1) + 0)) & 1) << (32 - (i + 1));
			}

			decodex[ix] = xx;
			decodey[ix] = yy;
		}

		initialized = 1;
	}

	*wx = *wy = 0;

	for (size_t i = 0; i < 8; i++) {
		*wx |= ((uint32_t) decodex[(index >> (8 * i)) & 0xFF]) << (4 * i);
		*wy |= ((uint32_t) decodey[(index >> (8 * i)) & 0xFF]) << (4 * i);
	}
}

void set_projection_or_exit(const char *optarg) {
	struct projection *p;
	for (p = projections; p->name != NULL; p++) {
		if (strcmp(p->name, optarg) == 0) {
			projection = p;
			break;
		}
		if (strcmp(p->alias, optarg) == 0) {
			projection = p;
			break;
		}
	}
	if (p->name == NULL) {
		fprintf(stderr, "Unknown projection (-s): %s\n", optarg);
		exit(EXIT_FAILURE);
	}
}
