#ifndef MAIN_HPP
#define MAIN_HPP

#include <stddef.h>

struct index {
	long long start;
	long long end;
	unsigned long long index;
	short segment;
	unsigned short t : 2;
	unsigned long long seq : (64 - 18);  // pack with segment and t to stay in 32 bytes
};

void checkdisk(struct reader *r, int nreader);

extern int geometry_scale;
extern int quiet;

extern size_t CPUS;
extern size_t TEMP_FILES;

extern size_t max_tile_size;

#define MAX_ZOOM 24

#endif
