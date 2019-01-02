#ifndef POOL_HPP
#define POOL_HPP

struct stringpool {
	uint64_t left = 0;
	uint64_t right = 0;
	uint64_t off = 0;
};

int64_t addpool(struct memfile *poolfile, struct memfile *treefile, const char *s, char type);

#endif
