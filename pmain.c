#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

static int n;
static int r;
static int m;
static int d;
static pthread_t *t;
static sem_t     *s;

static void loop(void *arg)
{
	int idx  = (long)arg;
	int next = idx / n * n + (idx + 1) % n;
	int i;

	sem_wait(&s[idx]);
	for (i = 0; i < m; ++i) {
		if (idx % n == i % n) {
			//printf("*%i: wake %i\n", idx, next);
			sem_post(&s[next]);
			//printf("*%i: wait\n", idx);
			sem_wait(&s[idx]);
		} else {
			//printf(" %i: wait\n", idx);
			sem_wait(&s[idx]);
			//printf(" %i: wake %i\n", idx, next);
			sem_post(&s[next]);
		}
	}
}

static void *f(void *arg)
{
	char pad[d];
	memset(pad, '#', d);
	loop(arg);
	return NULL;
}

int main(int argc, char **argv)
{
	uint64_t start;
	uint64_t end;
	struct timeval tt;
	int result;
	n = atoi(argv[1]); /* Cycle length. */
	r = atoi(argv[2]); /* Number of cycles. */
	m = atoi(argv[3]); /* Number of rounds. */
	d = atoi(argv[4]); /* Additional stack depth. */
	t = calloc(n * r, sizeof t[0]);
	s = calloc(n * r, sizeof s[0]);
	assert(t != NULL);
	assert(s != NULL);
	for (int i = 0; i < n * r; ++i) {
		result = sem_init(&s[i], 0, 0);
		assert(result == 0);
		result = pthread_create(&t[i], NULL, &f, (void *)(long)i);
		assert(result == 0);
	}
	gettimeofday(&tt, NULL);
	start = 1000000 * tt.tv_sec + tt.tv_usec;
	for (int i = 0; i < n * r; ++i) {
		result = sem_post(&s[i]);
	}
	gettimeofday(&tt, NULL);
	end = 1000000 * tt.tv_sec + tt.tv_usec;
	printf("%6i %6i %6i %f\n", n, r, m, (end - start) / 1000000.);
	free(t);
	return 0;
}
