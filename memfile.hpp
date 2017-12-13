#ifndef MEMFILE_HPP
#define MEMFILE_HPP

struct memfile {
	int fd = 0;
	char *map = NULL;
	size_t len = 0;
	size_t off = 0;
	size_t tree = 0;
};

struct memfile *memfile_open(int fd);
int memfile_close(struct memfile *file);
ssize_t memfile_write(struct memfile *file, void *s, size_t len);

#endif
