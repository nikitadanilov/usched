#include "rr.h"
#include <stdlib.h>
#include <stdio.h>

static int n;
static int m;
static struct rr_thread **t;

static void f(void *arg)
{
	int               idx  = (long)arg;
	struct rr_thread *next = t[(idx + 1) % n];
	int               i;

	for (i = 0; i < m; ++i) {
		if (idx == m % n) {
			rr_wake(next);
			rr_wait();
		} else {
			rr_wait();
			rr_wake(next);
		}
	}
}

int main(int argc, char **argv)
{
	n = atoi(argv[1]);
	m = atoi(argv[2]);
	t = calloc(n, sizeof t[0]);
	rr_init();
	for (int i = 0; i < n; ++i) {
		t[i] = rr_thread_init(f, (void *)(long)i);
	}
	rr_start();
	rr_fini();
	free(t);
}
