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
	pthread_attr_t attr;
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
	pthread_attr_init(&attr);
	if (d < 128 * 1024)
		d = 128 * 1024;
	result = pthread_attr_setstacksize(&attr, d);
	assert(result == 0);
	result = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	assert(result == 0);
	for (int i = 0; i < n * r; ++i) {
		result = sem_init(&s[i], 0, 0);
		assert(result == 0);
		result = pthread_create(&t[i], &attr, &f, (void *)(long)i);
		assert(result == 0);
	}
	gettimeofday(&tt, NULL);
	start = 1000000 * tt.tv_sec + tt.tv_usec;
	for (int i = 0; i < n * r; ++i) {
		result = sem_post(&s[i]);
	}
	for (int i = 0; i < n * r; ++i) {
		pthread_join(t[i], NULL);
	}
	gettimeofday(&tt, NULL);
	end = 1000000 * tt.tv_sec + tt.tv_usec;
	printf("%f\n", (end - start) / 1000000.);
	free(t);
	return 0;
}
