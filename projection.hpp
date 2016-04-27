void latlon2tile(double lat, double lon, int zoom, long long *x, long long *y);
void tile2latlon(long long x, long long y, int zoom, double *lat, double *lon);
unsigned long long encode(unsigned int wx, unsigned int wy);
void decode(unsigned long long index, unsigned *wx, unsigned *wy);
