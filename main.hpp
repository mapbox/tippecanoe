#ifndef MAIN_HPP
#define MAIN_HPP

#include <stddef.h>
#include <atomic>
#include <string>

struct index {
	int64_t start = 0;
	int64_t end = 0;
	uint64_t ix = 0;
	short segment = 0;
	uint16_t t : 2;
	uint64_t seq : (64 - 18);  // pack with segment and t to stay in 32 bytes

	index()
	    : t(0),
	      seq(0) {
	}
};

struct clipbbox {
	double lon1;
	double lat1;
	double lon2;
	double lat2;

	int64_t minx;
	int64_t miny;
	int64_t maxx;
	int64_t maxy;
};

extern std::vector<clipbbox> clipbboxes;

void checkdisk(std::vector<struct reader> *r);

extern int32_t geometry_scale;
extern int32_t quiet;
extern int32_t quiet_progress;
extern double progress_interval;
extern std::atomic<double> last_progress;

extern size_t CPUS;
extern size_t TEMP_FILES;

extern size_t max_tile_size;
extern size_t max_tile_features;
extern int32_t cluster_distance;
extern std::string attribute_for_id;

int32_t mkstemp_cloexec(char *name);
FILE *fopen_oflag(const char *name, const char *mode, int32_t oflag);
bool progress_time();

#define MAX_ZOOM 24

#endif
