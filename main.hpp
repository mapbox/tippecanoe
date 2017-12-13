#ifndef MAIN_HPP
#define MAIN_HPP

#include <stddef.h>

struct index {
	off_t start = 0;
	off_t end = 0;
	unsigned long ix = 0;
	unsigned short segment = 0;  // size_t elsewhere
	unsigned short t : 2;
	unsigned long seq : (64 - 18);  // pack with segment and t to stay in 32 bytes

	index()
	    : t(0),
	      seq(0) {
	}
};

void checkdisk(std::vector<struct reader> *r);

extern int geometry_scale;
extern bool quiet;
extern bool quiet_progress;

extern size_t CPUS;
extern size_t TEMP_FILES;

extern size_t max_tile_size;

int mkstemp_cloexec(char *name);
FILE *fopen_oflag(const char *name, const char *mode, int oflag);

#define MAX_ZOOM 24

#endif
