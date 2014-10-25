void latlon2tile(double lat, double lon, int zoom, unsigned int *x, unsigned int *y);
void tile2latlon(unsigned int x, unsigned int y, int zoom, double *lat, double *lon);
unsigned long long encode(unsigned int wx, unsigned int wy);
void decode(unsigned long long index, unsigned *wx, unsigned *wy);
