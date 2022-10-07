#include "ll.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <inttypes.h>

static int n;
static int r;
static int m;
static int d;
static struct ll_thread **t;

static void loop(void *arg)
{
	int idx  = (long)arg;
	int next = idx / n * n + (idx + 1) % n;
	int i;

	for (i = 0; i < m; ++i) {
		if (idx % n == i % n) {
			ll_wake(t[next]);
			ll_wait();
		} else {
			ll_wait();
			ll_wake(t[next]);
		}
	}
	ll_done();
}

static
#if defined(__clang__)
__attribute__((optnone))
#else
__attribute__((optimize("O0"))) /* keep pad[] */
#endif
void f(void *arg)
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
	int p = atoi(argv[5]);
	n = atoi(argv[1]); /* Cycle length. */
	r = atoi(argv[2]); /* Number of cycles. */
	m = atoi(argv[3]); /* Number of rounds. */
	d = atoi(argv[4]); /* Additional stack depth. */
	t = calloc(n * r, sizeof t[0]);
	if (p > r)
		p = r;
	ll_init(p /* processors */, n * r /* threads */);
	for (int i = 0; i < n * r; ++i) {
		t[i] = ll_thread_init(f, (void *)(long)i, i / n);
	}
	gettimeofday(&tt, NULL);
	start = 1000000 * tt.tv_sec + tt.tv_usec;
	ll_start();
	ll_fini();
	gettimeofday(&tt, NULL);
	end = 1000000 * tt.tv_sec + tt.tv_usec;
	printf("%f\n", (end - start) / 1000000.);
	free(t);
	return 0;
}
