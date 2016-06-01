struct index {
	long long start;
	long long end;
	unsigned long long index;
	short segment;
	unsigned long long seq : (64 - 16);  // pack with segment to stay in 32 bytes
};

void checkdisk(struct reader *r, int nreader);

extern int geometry_scale;
extern int quiet;

extern int CPUS;
extern int TEMP_FILES;

extern void (*projection)(double ix, double iy, int zoom, long long *ox, long long *oy);
