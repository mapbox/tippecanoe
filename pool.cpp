#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "memfile.hpp"
#include "pool.hpp"

#define POOL_WIDTH 256

static int hash(const char *s) {
	unsigned h = 0;
	for (; *s; s++) {
		h = (h * 37 + *s) & ULONG_MAX;
	}
	return h & 0xFF;
}

struct pool_val *pool(struct pool *p, const char *s, int type) {
	int h = hash(s);
	struct pool_val **v = &(p->vals[h]);

	while (*v != NULL) {
		int cmp = strcmp(s, (*v)->s);

		if (cmp == 0) {
			cmp = type - (*v)->type;
		}

		if (cmp == 0) {
			return *v;
		} else if (cmp < 0) {
			v = &((*v)->left);
		} else {
			v = &((*v)->right);
		}
	}

	struct pool_val *nv = (struct pool_val *) malloc(sizeof(struct pool_val));
	if (nv == NULL) {
		fprintf(stderr, "out of memory making string pool\n");
		exit(EXIT_FAILURE);
	}
	nv->left = NULL;
	nv->right = NULL;
	nv->next = NULL;
	nv->s = s;
	nv->type = type;
	nv->n = p->n++;

	if (p->tail != NULL) {
		p->tail->next = nv;
	}
	p->tail = nv;
	if (p->head == NULL) {
		p->head = nv;
	}

	*v = nv;
	return *v;
}

int is_pooled(struct pool *p, const char *s, int type) {
	int h = hash(s);
	struct pool_val **v = &(p->vals[h]);

	while (*v != NULL) {
		int cmp = strcmp(s, (*v)->s);

		if (cmp == 0) {
			cmp = type - (*v)->type;
		}

		if (cmp == 0) {
			return 1;
		} else if (cmp < 0) {
			v = &((*v)->left);
		} else {
			v = &((*v)->right);
		}
	}

	return 0;
}

void pool_free1(struct pool *p, void (*func)(void *)) {
	while (p->head != NULL) {
		if (func != NULL) {
			func((void *) p->head->s);
		}

		struct pool_val *next = p->head->next;
		free(p->head);
		p->head = next;
	}

	p->head = NULL;
	p->tail = NULL;

	free(p->vals);
	p->vals = NULL;
}

void pool_free(struct pool *p) {
	pool_free1(p, NULL);
}

void pool_free_strings(struct pool *p) {
	pool_free1(p, free);
}

void pool_init(struct pool *p, int n) {
	p->n = n;
	p->vals = (struct pool_val **) calloc(POOL_WIDTH, sizeof(struct pool_val *));
	if (p->vals == NULL) {
		fprintf(stderr, "out of memory creating string pool\n");
		exit(EXIT_FAILURE);
	}
	p->head = NULL;
	p->tail = NULL;
}

static unsigned char swizzle[256] = {
	0x00, 0xBF, 0x18, 0xDE, 0x93, 0xC9, 0xB1, 0x5E, 0xDF, 0xBE, 0x72, 0x5A, 0xBB, 0x42, 0x64, 0xC6,
	0xD8, 0xB7, 0x15, 0x74, 0x1C, 0x8B, 0x91, 0xF5, 0x29, 0x46, 0xEC, 0x6F, 0xCA, 0x20, 0xF0, 0x06,
	0x27, 0x61, 0x87, 0xE0, 0x6E, 0x43, 0x50, 0xC5, 0x1B, 0xB4, 0x37, 0xC3, 0x69, 0xA6, 0xEE, 0x80,
	0xAF, 0x9B, 0xA1, 0x76, 0x23, 0x24, 0x53, 0xF3, 0x5B, 0x65, 0x19, 0xF4, 0xFC, 0xDD, 0x26, 0xE8,
	0x10, 0xF7, 0xCE, 0x92, 0x48, 0xF6, 0x94, 0x60, 0x07, 0xC4, 0xB9, 0x97, 0x6D, 0xA4, 0x11, 0x0D,
	0x1F, 0x4D, 0x13, 0xB0, 0x5D, 0xBA, 0x31, 0xD5, 0x8D, 0x51, 0x36, 0x96, 0x7A, 0x03, 0x7F, 0xDA,
	0x17, 0xDB, 0xD4, 0x83, 0xE2, 0x79, 0x6A, 0xE1, 0x95, 0x38, 0xFF, 0x28, 0xB2, 0xB3, 0xA7, 0xAE,
	0xF8, 0x54, 0xCC, 0xDC, 0x9A, 0x6B, 0xFB, 0x3F, 0xD7, 0xBC, 0x21, 0xC8, 0x71, 0x09, 0x16, 0xAC,
	0x3C, 0x8A, 0x62, 0x05, 0xC2, 0x8C, 0x32, 0x4E, 0x35, 0x9C, 0x5F, 0x75, 0xCD, 0x2E, 0xA2, 0x3E,
	0x1A, 0xC1, 0x8E, 0x14, 0xA0, 0xD3, 0x7D, 0xD9, 0xEB, 0x5C, 0x70, 0xE6, 0x9E, 0x12, 0x3B, 0xEF,
	0x1E, 0x49, 0xD2, 0x98, 0x39, 0x7E, 0x44, 0x4B, 0x6C, 0x88, 0x02, 0x2C, 0xAD, 0xE5, 0x9F, 0x40,
	0x7B, 0x4A, 0x3D, 0xA9, 0xAB, 0x0B, 0xD6, 0x2F, 0x90, 0x2A, 0xB6, 0x1D, 0xC7, 0x22, 0x55, 0x34,
	0x0A, 0xD0, 0xB5, 0x68, 0xE3, 0x59, 0xFD, 0xFA, 0x57, 0x77, 0x25, 0xA3, 0x04, 0xB8, 0x33, 0x89,
	0x78, 0x82, 0xE4, 0xC0, 0x0E, 0x8F, 0x85, 0xD1, 0x84, 0x08, 0x67, 0x47, 0x9D, 0xCB, 0x58, 0x4C,
	0xAA, 0xED, 0x52, 0xF2, 0x4F, 0xF1, 0x66, 0xCF, 0xA5, 0x56, 0xEA, 0x7C, 0xE9, 0x63, 0xE7, 0x01,
	0xF9, 0xFE, 0x0C, 0x99, 0x2D, 0x0F, 0x3A, 0x41, 0x45, 0xA8, 0x30, 0x2B, 0x73, 0xBD, 0x86, 0x81,
};

int swizzlecmp(const char *a, const char *b) {
	while (*a || *b) {
		int aa = swizzle[(unsigned char) *a];
		int bb = swizzle[(unsigned char) *b];

		int cmp = aa - bb;
		if (cmp != 0) {
			return cmp;
		}

		a++;
		b++;
	}

	return 0;
}

long long addpool(struct memfile *poolfile, struct memfile *treefile, const char *s, char type) {
	long long *sp = &treefile->tree;

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
