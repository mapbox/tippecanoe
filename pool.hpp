#ifndef POOL_HPP
#define POOL_HPP

struct stringpool {
	unsigned long left;
	unsigned long right;
	unsigned long off;
};

long long addpool(struct memfile *poolfile, struct memfile *treefile, const char *s, char type);

#endif
