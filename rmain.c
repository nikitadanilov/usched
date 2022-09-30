#include "rr.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>

static int n;
static int r;
static int m;
static int d;
static struct rr_thread **t;

static void loop(void *arg)
{
	int idx  = (long)arg;
	int next = idx / n * n + (idx + 1) % n;
	int i;

	for (i = 0; i < m; ++i) {
		if (idx % n == i % n) {
			//printf("*%i: wake %i\n", idx, next);
			rr_wake(t[next]);
			//printf("*%i: wait\n", idx);
			rr_wait();
		} else {
			//printf(" %i: wait\n", idx);
			rr_wait();
			//printf(" %i: wake %i\n", idx, next);
			rr_wake(t[next]);
		}
	}
}

static void f(void *arg)
{
	char pad[d];
	memset(pad, '#', d);
	loop(arg);
}

int main(int argc, char **argv)
{
	uint64_t start;
	uint64_t end;
	struct timeval tt;
	n = atoi(argv[1]); /* Cycle length. */
	r = atoi(argv[2]); /* Number of cycles. */
	m = atoi(argv[3]); /* Number of rounds. */
	d = atoi(argv[4]); /* Additional stack depth. */
	t = calloc(n * r, sizeof t[0]);
	rr_init(atoi(argv[5]) /* processors */, n * r /* threads */);
	for (int i = 0; i < n * r; ++i) {
		t[i] = rr_thread_init(f, (void *)(long)i);
	}
	gettimeofday(&tt, NULL);
	start = 1000000 * tt.tv_sec + tt.tv_usec;
	rr_start();
	rr_fini();
	gettimeofday(&tt, NULL);
	end = 1000000 * tt.tv_sec + tt.tv_usec;
	printf("%6i %6i %6i %f\n", n, r, m, (end - start) / 1000000.);
	free(t);
	return 0;
}
