#include "rr.h"
#include "usched.h"

#include <pthread.h>
#include <stdlib.h>
#include <assert.h>

enum {
	NR_PROCESSORS = 8,
	NR_THREADS    = 16 * 1024
};

struct rr_thread {
	struct ustack r_stack; /* Must go first. */
	int           r_idx;
	int           r_nr_wake;
};

struct processor {
	struct usched     p_sched; /* Must go first. */
	int               p_exit;
	pthread_t         p_thread;
	pthread_mutex_t   p_lock;
	pthread_cond_t    p_todo;
	int               p_nr_ready;
	int               p_nr_wait;
	struct rr_thread *p_run;
	struct rr_thread *p_ready[NR_THREADS]; /* In good old UNIX tradition */
	struct rr_thread *p_wait[NR_THREADS];  /* pre-allocate everything. */
};

static struct processor p[NR_PROCESSORS] = {};
static int nr_t = 0;

enum state { WAIT, READY, RUN };

static void  proc_init(struct processor *p);
static void  proc_fini(struct processor *p);
static void *proc(void *arg);

static enum state rr_state(struct rr_thread *t)
{
	struct processor *p = (void *)t->r_stack.u_sched;
	if (t == p->p_run)
		return RUN;
	else if (t->r_idx < p->p_nr_wait && p->p_wait[t->r_idx] == t)
		return WAIT;
	else
		return READY;
}

int rr_init(void)
{
	int i;
	for (i = 0; i < NR_PROCESSORS; ++i) {
		proc_init(&p[i]);
	}
	return 0;
}

void rr_fini(void)
{
	int i;
	for (i = 0; i < NR_PROCESSORS; ++i) {
		proc_fini(&p[i]);
	}
}

struct rr_thread *rr_thread_init(void (*f)(void *), void *arg)
{
	struct rr_thread *t = calloc(1, sizeof *t);
	if (t != NULL) {
		struct processor *proc = &p[nr_t++ % NR_PROCESSORS];
		ustack_init(&t->r_stack, &proc->p_sched, f, arg, NULL, 0);
		pthread_mutex_lock(&p->p_lock);
		assert(p->p_nr_ready < NR_THREADS);
		p->p_ready[p->p_nr_ready++] = t;
		pthread_cond_signal(&p->p_todo);
		pthread_mutex_unlock(&p->p_lock);
	}
	return t;
}

void rr_wait(void)
{
	struct ustack    *u = ustack_self();
	struct rr_thread *t = (void *)u;
	struct processor *p = (void *)u->u_sched;
	pthread_mutex_lock(&p->p_lock);
	assert(t == p->p_run);
	if (t->r_nr_wake == 0) {
		assert(p->p_nr_wait < NR_THREADS);
		t->r_idx = p->p_nr_wait;
		p->p_wait[p->p_nr_wait++] = t;
		p->p_run = NULL;
	} else
		t->r_nr_wake--;
	pthread_mutex_unlock(&p->p_lock);
	ustack_block();
}

void rr_wake(struct rr_thread *t)
{
	struct processor *p = (void *)t->r_stack.u_sched;
	pthread_mutex_lock(&p->p_lock);
	if (rr_state(t) == WAIT) {
		assert(p->p_nr_ready < NR_THREADS);
		--p->p_nr_wait;
		p->p_wait[t->r_idx] = p->p_wait[p->p_nr_wait];
		p->p_ready[p->p_nr_ready] = t;
		++p->p_nr_ready;
		pthread_cond_signal(&p->p_todo);
	} else
		t->r_nr_wake++;
	pthread_mutex_unlock(&p->p_lock);
}

struct ustack *rr_next(struct usched *s)
{
	struct processor *p = (void *)s;
	if (p->p_exit && p->p_nr_ready == 0 && p->p_nr_wait == 0)
		return NULL; /* Safe without mutex. */
	pthread_mutex_lock(&p->p_lock);
	while (p->p_nr_ready == 0)
		pthread_cond_wait(&p->p_todo, &p->p_lock);
	p->p_run = p->p_ready[--p->p_nr_ready];
	pthread_mutex_unlock(&p->p_lock);
	return &p->p_run->r_stack;
}

void *rr_alloc(struct usched *s, int size)
{
	return malloc(size);
}

void rr_free(struct usched *s, void *addr, int size)
{
	free(addr);
}

static void proc_init(struct processor *p)
{
	pthread_mutex_init(&p->p_lock, NULL);
	pthread_cond_init(&p->p_todo, NULL);
	p->p_sched.s_next  = &rr_next;
	p->p_sched.s_alloc = &rr_alloc;
	p->p_sched.s_free  = &rr_free;
	pthread_create(&p->p_thread, NULL, &proc, p);
}

static void proc_fini(struct processor *p)
{
	p->p_exit = 1;
	pthread_cond_signal(&p->p_todo);
	pthread_join(p->p_thread, NULL);
	pthread_cond_destroy(&p->p_todo);
	pthread_mutex_destroy(&p->p_lock);
}

static void *proc(void *arg)
{
	struct processor *p = arg;
	usched_run(&p->p_sched);
	return NULL;
}
