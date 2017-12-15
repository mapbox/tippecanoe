#ifndef POOL_HPP
#define POOL_HPP

struct stringpool {
	unsigned long left = 0;
	unsigned long right = 0;
	size_t off = 0;
};

size_t addpool(struct memfile *poolfile, struct memfile *treefile, const char *s, char type);

#endif
