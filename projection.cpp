#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "projection.hpp"

struct projection projections[] = {
	{"EPSG:4326", lonlat2tile, tile2lonlat, "urn:ogc:def:crs:OGC:1.3:CRS84"},
	{"EPSG:3857", epsg3857totile, tiletoepsg3857, "urn:ogc:def:crs:EPSG::3857"},
	{NULL, NULL, NULL, NULL},
};

struct projection *projection = &projections[0];

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
void lonlat2tile(double lon, double lat, int zoom, long *x, long *y) {
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
	unsigned long n = 1L << zoom;

	long llx = (long) floor(n * ((lon + 180) / 360));
	long lly = (long) floor(n * (1 - (log(tan(lat_rad) + 1 / cos(lat_rad)) / M_PI)) / 2);

	*x = llx;
	*y = lly;
}

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
void tile2lonlat(long x, long y, int zoom, double *lon, double *lat) {
	unsigned long n = 1L << zoom;
	*lon = 360.0 * x / n - 180.0;
	*lat = atan(sinh(M_PI * (1 - 2.0 * y / n))) * 180.0 / M_PI;
}

void epsg3857totile(double ix, double iy, int zoom, long *x, long *y) {
	*x = (long) floor(ix * (1L << 31) / 6378137.0 / M_PI + (1L << 31));
	*y = (long) floor(((1L << 32) - 1) - (iy * (1L << 31) / 6378137.0 / M_PI + (1L << 31)));

	if (zoom != 0) {
		*x >>= (32 - zoom);
		*y >>= (32 - zoom);
	}
}

void tiletoepsg3857(long ix, long iy, int zoom, double *ox, double *oy) {
	if (zoom != 0) {
		ix <<= (32 - zoom);
		iy <<= (32 - zoom);
	}

	*ox = (ix - (1L << 31)) * M_PI * 6378137.0 / (1L << 31);
	*oy = ((1L << 32) - 1 - iy - (1L << 31)) * M_PI * 6378137.0 / (1L << 31);
}

unsigned long encode(unsigned wx, unsigned wy) {
	unsigned long out = 0;

	size_t i;
	for (i = 0; i < 32; i++) {
		unsigned long v = ((wx >> (32 - (i + 1))) & 1) << 1;
		v |= (wy >> (32 - (i + 1))) & 1;
		v = v << (64 - 2 * (i + 1));

		out |= v;
	}

	return out;
}

static unsigned char decodex[256];
static unsigned char decodey[256];

void decode(unsigned long index, unsigned *wx, unsigned *wy) {
	static bool initialized = false;
	if (!initialized) {
		// This is an optimization to precalculate the output nibbles that correspond
		// to each possible input byte so each bit doesn't have to be shifted into place
		// individually every time.

		for (size_t ix = 0; ix < 256; ix++) {
			size_t xx = 0, yy = 0;

			for (size_t i = 0; i < 32; i++) {
				xx |= ((ix >> (64 - 2 * (i + 1) + 1)) & 1) << (32 - (i + 1));
				yy |= ((ix >> (64 - 2 * (i + 1) + 0)) & 1) << (32 - (i + 1));
			}

			decodex[ix] = (unsigned char) xx;
			decodey[ix] = (unsigned char) yy;
		}

		initialized = 1;
	}

	*wx = *wy = 0;

	for (size_t i = 0; i < 8; i++) {
		*wx |= ((unsigned) decodex[(index >> (8 * i)) & 0xFF]) << (4 * i);
		*wy |= ((unsigned) decodey[(index >> (8 * i)) & 0xFF]) << (4 * i);
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
