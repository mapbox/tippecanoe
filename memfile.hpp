#ifndef MEMFILE_HPP
#define MEMFILE_HPP

#include <atomic>

struct memfile {
	int fd = 0;
	char *map = NULL;
	std::atomic<long long> len;
	long long off = 0;
	unsigned long tree = 0;

	memfile()
	    : len(0) {
	}
};

struct memfile *memfile_open(int fd);
int memfile_close(struct memfile *file);
int memfile_write(struct memfile *file, void *s, long long len);

#endif
