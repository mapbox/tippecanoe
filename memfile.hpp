#ifndef MEMFILE_HPP
#define MEMFILE_HPP

struct memfile {
	int fd;
	char *map;
	long long len;
	long long off;
	unsigned long tree;
};

struct memfile *memfile_open(int fd);
int memfile_close(struct memfile *file);
int memfile_write(struct memfile *file, void *s, long long len);

#endif
