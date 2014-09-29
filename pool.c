#include <stdlib.h>
#include <string.h>
#include "pool.h"

static struct pool_val *pool1(struct pool *p, char *s, int type, int (*compare)(const char *, const char *)) {
	struct pool_val **v = &(p->vals);

	while (*v != NULL) {
		int cmp = compare(s, (*v)->s);

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

	*v = malloc(sizeof(struct pool_val));
	(*v)->left = NULL;
	(*v)->right = NULL;
	(*v)->next = NULL;
	(*v)->s = s;
	(*v)->type = type;
	(*v)->n = p->n++;

	if (p->tail != NULL) {
		p->tail->next = *v;
	}
	p->tail = *v;
	if (p->head == NULL) {
		p->head = *v;
	}

	return *v;
}

int is_pooled(struct pool *p, char *s, int type) {
	struct pool_val **v = &(p->vals);

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


struct pool_val *pool(struct pool *p, char *s, int type) {
	return pool1(p, s, type, strcmp);
}

static int llcmp(const char *v1, const char *v2) {
	long long *ll1 = (long long *) v1;
	long long *ll2 = (long long *) v2;

	if (*ll1 < *ll2) {
		return -1;
	} else if (*ll1 > *ll2) {
		return 1;
	} else {
		return 0;
	}
}

struct pool_val *pool_long_long(struct pool *p, long long *s, int type) {
	return pool1(p, (char *) s, type, llcmp);
}

void pool_free(struct pool *p) {
	while (p->head != NULL) {
		struct pool_val *next = p->head->next;
		free(p->head);
		p->head = next;
	}

	p->head = NULL;
	p->tail = NULL;
	p->vals = NULL;
}

void pool_init(struct pool *p, int n) {
	p->n = n;
	p->vals = NULL;
	p->head = NULL;
	p->tail = NULL;
}
