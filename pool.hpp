#ifndef POOL_HPP
#define POOL_HPP

struct stringpool {
	unsigned long left = 0;
	unsigned long right = 0;
	unsigned long off = 0;
};

long long addpool(struct memfile *poolfile, struct memfile *treefile, const char *s, char type);

#endif
