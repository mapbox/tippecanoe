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

void pool_free1(struct pool *p, void (*func)(void *)) {
	while (p->head != NULL) {
		if (func != NULL) {
			func(p->head->s);
		}

		struct pool_val *next = p->head->next;
		free(p->head);
		p->head = next;
	}

	p->head = NULL;
	p->tail = NULL;
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
	p->vals = NULL;
	p->head = NULL;
	p->tail = NULL;
}
