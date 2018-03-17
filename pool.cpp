#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "memfile.hpp"
#include "pool.hpp"

int swizzlecmp(const char *a, const char *b) {
	ssize_t alen = strlen(a);
	ssize_t blen = strlen(b);

	if (strcmp(a, b) == 0) {
		return 0;
	}

	long long hash1 = 0, hash2 = 0;
	for (ssize_t i = alen - 1; i >= 0; i--) {
		hash1 = (hash1 * 37 + a[i]) & INT_MAX;
	}
	for (ssize_t i = blen - 1; i >= 0; i--) {
		hash2 = (hash2 * 37 + b[i]) & INT_MAX;
	}

	int h1 = hash1, h2 = hash2;
	if (h1 == h2) {
		return strcmp(a, b);
	}

	return h1 - h2;
}

long long addpool(struct memfile *poolfile, struct memfile *treefile, const char *s, char type) {
	unsigned long *sp = &treefile->tree;
	size_t depth = 0;

	// In typical data, traversal depth generally stays under 2.5x
	size_t max = 3 * log(treefile->off / sizeof(struct stringpool)) / log(2);
	if (max < 30) {
		max = 30;
	}

	while (*sp != 0) {
		int cmp = swizzlecmp(s, poolfile->map + ((struct stringpool *) (treefile->map + *sp))->off + 1);

		if (cmp == 0) {
			cmp = type - (poolfile->map + ((struct stringpool *) (treefile->map + *sp))->off)[0];
		}

		if (cmp < 0) {
			sp = &(((struct stringpool *) (treefile->map + *sp))->left);
		} else if (cmp > 0) {
			sp = &(((struct stringpool *) (treefile->map + *sp))->right);
		} else {
			return ((struct stringpool *) (treefile->map + *sp))->off;
		}

		depth++;
		if (depth > max) {
			// Search is very deep, so string is probably unique.
			// Add it to the pool without adding it to the search tree.

			long long off = poolfile->off;
			if (memfile_write(poolfile, &type, 1) < 0) {
				perror("memfile write");
				exit(EXIT_FAILURE);
			}
			if (memfile_write(poolfile, (void *) s, strlen(s) + 1) < 0) {
				perror("memfile write");
				exit(EXIT_FAILURE);
			}
			return off;
		}
	}

	// *sp is probably in the memory-mapped file, and will move if the file grows.
	long long ssp;
	if (sp == &treefile->tree) {
		ssp = -1;
	} else {
		ssp = ((char *) sp) - treefile->map;
	}

	long long off = poolfile->off;
	if (memfile_write(poolfile, &type, 1) < 0) {
		perror("memfile write");
		exit(EXIT_FAILURE);
	}
	if (memfile_write(poolfile, (void *) s, strlen(s) + 1) < 0) {
		perror("memfile write");
		exit(EXIT_FAILURE);
	}

	if (off >= LONG_MAX || treefile->off >= LONG_MAX) {
		// Tree or pool is bigger than 2GB
		static bool warned = false;
		if (!warned) {
			fprintf(stderr, "Warning: string pool is very large.\n");
			warned = true;
		}
		return off;
	}

	struct stringpool tsp;
	tsp.left = 0;
	tsp.right = 0;
	tsp.off = off;

	long long p = treefile->off;
	if (memfile_write(treefile, &tsp, sizeof(struct stringpool)) < 0) {
		perror("memfile write");
		exit(EXIT_FAILURE);
	}

	if (ssp == -1) {
		treefile->tree = p;
	} else {
		*((long long *) (treefile->map + ssp)) = p;
	}
	return off;
}
