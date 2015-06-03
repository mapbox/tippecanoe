#include <math.h>
#include "projection.h"

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
void latlon2tile(double lat, double lon, int zoom, unsigned int *x, unsigned int *y) {
	double lat_rad = lat * M_PI / 180;
	unsigned long long n = 1LL << zoom;

	long long llx = n * ((lon + 180) / 360);
	long long lly = n * (1 - (log(tan(lat_rad) + 1 / cos(lat_rad)) / M_PI)) / 2;

	if (lat >= 85.0511) {
		lly = 0;
	}
	if (lat <= -85.0511) {
		lly = n - 1;
	}

	if (llx < 0) {
		llx = 0;
	}
	if (lly < 0) {
		lly = 0;
	}
	if (llx >= n) {
		llx = n - 1;
	}
	if (lly >= n) {
		lly = n - 1;
	}

	*x = llx;
	*y = lly;
}

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
void tile2latlon(unsigned int x, unsigned int y, int zoom, double *lat, double *lon) {
	unsigned long long n = 1LL << zoom;
	*lon = 360.0 * x / n - 180.0;
	float lat_rad = atan(sinh(M_PI * (1 - 2.0 * y / n)));
	*lat = lat_rad * 180 / M_PI;
}

unsigned long long encode(unsigned int wx, unsigned int wy) {
	long long out = 0;

	int i;
	for (i = 0; i < 32; i++) {
		long long v = ((wx >> (32 - (i + 1))) & 1) << 1;
		v |= (wy >> (32 - (i + 1))) & 1;
		v = v << (64 - 2 * (i + 1));

		out |= v;
	}

	return out;
}

void decode(unsigned long long index, unsigned *wx, unsigned *wy) {
	*wx = *wy = 0;

	int i;
	for (i = 0; i < 32; i++) {
		*wx |= ((index >> (64 - 2 * (i + 1) + 1)) & 1) << (32 - (i + 1));
		*wy |= ((index >> (64 - 2 * (i + 1) + 0)) & 1) << (32 - (i + 1));
	}
}
