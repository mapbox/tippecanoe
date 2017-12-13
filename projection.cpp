#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <string>
#include <ctype.h>
#include "projection.hpp"

struct wkt {
	std::string s;
	std::vector<wkt> sublist;

	wkt(std::string s_) {
		s = s_;
	}
};

static int peekc(FILE *f) {
	int c = getc(f);
	ungetc(c, f);
	return c;
}

static void parsewkt(FILE *f, std::vector<wkt> &w) {
	int c;

	while ((c = getc(f)) != EOF) {
		if (c == ' ' || c == '\t' || c == '\n' || c == ',') {
			continue;
		}

		if (c == '"') {
			std::string s;

			while ((c = getc(f)) != EOF) {
				if (c == '"' && peekc(f) == '"') {
					getc(f);
					s.push_back('"');
					continue;
				}

				if (c == '"') {
					break;
				}

				s.push_back(c);
			}

			w.push_back(wkt(s));
			continue;
		}

		if (isalpha(c)) {
			std::string s;
			s.push_back(tolower(c));

			while ((c = peekc(f)) != EOF && isalpha(c)) {
				s.push_back(tolower(c));
				c = getc(f);
			}

			w.push_back(wkt(s));
			continue;
		}

		if (isdigit(c) || c == '.' || c == '-') {
			std::string s;
			s.push_back(c);

			while ((c = peekc(f)) != EOF && (isdigit(c) || c == '.')) {
				s.push_back(c);
				c = getc(f);
			}

			w.push_back(wkt(s));
			continue;
		}

		if (c == ')' || c == ']') {
			return;
		}

		if (c == '(' || c == '[') {
			if (w.size() == 0) {
				fprintf(stderr, "[ or ( in projection without a leading token\n");
				exit(EXIT_FAILURE);
			}

			parsewkt(f, w[w.size() - 1].sublist);
			continue;
		}

		fprintf(stderr, "Unexpected character %c in projection\n", c);
		exit(EXIT_FAILURE);
	}
}

wkt *find(std::vector<wkt> *w, std::string s) {
	for (size_t i = 0; i < w->size(); i++) {
		if ((*w)[i].s == s) {
			return &(*w)[i];
		}
	}

	return NULL;
}

void warn_wgs84_inner(std::vector<wkt> *w) {
	wkt *geogcs = find(w, "geogcs");
	if (geogcs == NULL) {
		fprintf(stderr, "Warning: expected geographic coordinate system for projection WGS84\n");
		return;
	}

	wkt *datum = find(&(geogcs->sublist), "datum");
	if (datum == NULL) {
		fprintf(stderr, "Warning: expected datum in projection\n");
		return;
	}

	wkt *spheroid = find(&(datum->sublist), "spheroid");
	if (spheroid == NULL || spheroid->sublist.size() == 0) {
		fprintf(stderr, "Warning: expected spheroid in projection\n");
		return;
	}

	if (spheroid->sublist[0].s != "WGS_1984") {
		fprintf(stderr, "Warning: expected WGS_1984 spheroid in projection, not %s\n", spheroid->sublist[0].s.c_str());
		return;
	}
}

void warn_wgs84(FILE *f) {
	std::vector<wkt> w;
	parsewkt(f, w);

	warn_wgs84_inner(&w);
}

void warn_epsg3857(FILE *f) {
	std::vector<wkt> w;
	parsewkt(f, w);

	wkt *projcs = find(&w, "projcs");
	if (projcs == NULL) {
		fprintf(stderr, "Warning: expected projected coordinate system for projection EPSG:3857\n");
		return;
	}

	warn_wgs84_inner(&(projcs->sublist));
}

struct projection projections[] = {
	{"EPSG:4326", lonlat2tile, tile2lonlat, "urn:ogc:def:crs:OGC:1.3:CRS84", warn_wgs84},
	{"EPSG:3857", epsg3857totile, tiletoepsg3857, "urn:ogc:def:crs:EPSG::3857", warn_epsg3857},
	{NULL, NULL, NULL, NULL, NULL},
};

struct projection *projection = &projections[0];

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
void lonlat2tile(double lon, double lat, int zoom, long long *x, long long *y) {
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
	unsigned long long n = 1LL << zoom;

	long long llx = n * ((lon + 180) / 360);
	long long lly = n * (1 - (log(tan(lat_rad) + 1 / cos(lat_rad)) / M_PI)) / 2;

	*x = llx;
	*y = lly;
}

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
void tile2lonlat(long long x, long long y, int zoom, double *lon, double *lat) {
	unsigned long long n = 1LL << zoom;
	*lon = 360.0 * x / n - 180.0;
	*lat = atan(sinh(M_PI * (1 - 2.0 * y / n))) * 180.0 / M_PI;
}

void epsg3857totile(double ix, double iy, int zoom, long long *x, long long *y) {
	*x = ix * (1LL << 31) / 6378137.0 / M_PI + (1LL << 31);
	*y = ((1LL << 32) - 1) - (iy * (1LL << 31) / 6378137.0 / M_PI + (1LL << 31));

	if (zoom != 0) {
		*x >>= (32 - zoom);
		*y >>= (32 - zoom);
	}
}

void tiletoepsg3857(long long ix, long long iy, int zoom, double *ox, double *oy) {
	if (zoom != 0) {
		ix <<= (32 - zoom);
		iy <<= (32 - zoom);
	}

	*ox = (ix - (1LL << 31)) * M_PI * 6378137.0 / (1LL << 31);
	*oy = ((1LL << 32) - 1 - iy - (1LL << 31)) * M_PI * 6378137.0 / (1LL << 31);
}

unsigned long long encode(unsigned int wx, unsigned int wy) {
	unsigned long long out = 0;

	int i;
	for (i = 0; i < 32; i++) {
		unsigned long long v = ((wx >> (32 - (i + 1))) & 1) << 1;
		v |= (wy >> (32 - (i + 1))) & 1;
		v = v << (64 - 2 * (i + 1));

		out |= v;
	}

	return out;
}

static unsigned char decodex[256];
static unsigned char decodey[256];

void decode(unsigned long long index, unsigned *wx, unsigned *wy) {
	static int initialized = 0;
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
