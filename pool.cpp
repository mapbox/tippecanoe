#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
	#include "pool.h"
}

#define POOL_WIDTH 256

static int hash(const char *s) {
	int h = 0;
	for (; *s; s++) {
		h = h * 37 + *s;
	}
	h = h & 0xFF;
	return h;
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
