#ifndef POOL_HPP
#define POOL_HPP

struct stringpool {
	size_t left = 0;
	size_t right = 0;
	size_t off = 0;
};

size_t addpool(struct memfile *poolfile, struct memfile *treefile, const char *s, char type);

#endif
