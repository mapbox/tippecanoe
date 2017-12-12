#ifndef MEMFILE_HPP
#define MEMFILE_HPP

struct memfile {
	int fd = 0;
	char *map = NULL;
	long len = 0;
	long off = 0;
	unsigned long tree = 0;
};

struct memfile *memfile_open(int fd);
int memfile_close(struct memfile *file);
int memfile_write(struct memfile *file, void *s, long len);

#endif
