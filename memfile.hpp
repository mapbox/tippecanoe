#ifndef MEMFILE_HPP
#define MEMFILE_HPP

#include <atomic>

struct memfile {
	int32_t fd = 0;
	char *map = NULL;
	std::atomic<int64_t> len;
	int64_t off = 0;
	uint64_t tree = 0;

	memfile()
	    : len(0) {
	}
};

struct memfile *memfile_open(int32_t fd);
int32_t memfile_close(struct memfile *file);
int32_t memfile_write(struct memfile *file, void *s, int64_t len);

#endif
