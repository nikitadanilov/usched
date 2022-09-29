#include "usched.h"
#include <stdlib.h>
#include <stdio.h>

enum { NR = 10 };

static struct ustack u[NR] = {};
static int idx = 0;

static struct ustack *_next(struct usched *s)
{
	return &u[idx++ % NR];
}

static void *_alloc(struct usched *s, int size)
{
	return malloc(size);
}

static void _free(struct usched *s, void *addr, int size)
{
	return free(addr);
}

static void f(void *arg)
{
	int idx = (int)arg;
	int i = 0;
	while (1) {
		printf("%i:%i\n", idx, i);
		ustack_block();
		i++;
	}
}

int main(int argc, char **argv)
{
	int i;
	struct usched s = {
		.s_next  = &_next,
		.s_alloc = &_alloc,
		.s_free  = &_free
	};
	for (i = 0; i < NR; ++i) {
		ustack_init(&u[i], &s, &f, (void *)(long)i, NULL, 0, &u[i]);
	}
	usched_init(&s);
	usched_run(&s);
}
