#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "memfile.hpp"
#include "pool.hpp"

int swizzlecmp(const char *a, const char *b) {
	size_t alen = strlen(a);
	size_t blen = strlen(b);

	if (strcmp(a, b) == 0) {
		return 0;
	}

	// This is long to avoid complaints about overflow
	long hash1 = 0, hash2 = 0;
	for (size_t i = alen; i > 0; i--) {
		hash1 = (hash1 * 37 + a[i - 1]) & INT_MAX;
	}
	for (size_t i = blen; i > 0; i--) {
		hash2 = (hash2 * 37 + b[i - 1]) & INT_MAX;
	}

	int h1 = (int) hash1, h2 = (int) hash2;
	if (h1 == h2) {
		return strcmp(a, b);
	}

	return h1 - h2;
}

size_t addpool(struct memfile *poolfile, struct memfile *treefile, const char *s, char type) {
	size_t *sp = &treefile->tree;
	size_t depth = 0;

	// In typical data, traversal depth generally stays under 2.5x
	size_t max = (size_t) floor(3 * log(treefile->off / sizeof(struct stringpool)) / log(2));
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

			size_t off = poolfile->off;
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
	ssize_t ssp;
	bool is_root = false;
	if (sp == &treefile->tree) {
		ssp = 0;
		is_root = true;
	} else {
		ssp = ((char *) sp) - treefile->map;
	}

	size_t off = poolfile->off;
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

	size_t p = treefile->off;
	if (memfile_write(treefile, &tsp, sizeof(struct stringpool)) < 0) {
		perror("memfile write");
		exit(EXIT_FAILURE);
	}

	if (is_root) {
		treefile->tree = p;
	} else {
		*((size_t *) (treefile->map + ssp)) = p;
	}
	return off;
}
