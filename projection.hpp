void lonlat2tile(double lon, double lat, int zoom, long long *x, long long *y);
void epsg3857totile(double ix, double iy, int zoom, long long *x, long long *y);
void tile2lonlat(long long x, long long y, int zoom, double *lon, double *lat);
unsigned long long encode(unsigned int wx, unsigned int wy);
void decode(unsigned long long index, unsigned *wx, unsigned *wy);
