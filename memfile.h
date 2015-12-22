struct memfile {
	int fd;
	char *map;
	long long len;
	long long off;
	long long tree;
};

struct memfile *memfile_open(int fd);
int memfile_close(struct memfile *file);
int memfile_write(struct memfile *file, void *s, long long len);
