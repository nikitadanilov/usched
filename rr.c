#include "rr.h"
#include "usched.h"

#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

struct rr_thread {
	struct ustack r_stack; /* Must go first. */
	int           r_idx;
	int           r_nr_wake;
};

struct processor {
	struct usched      p_sched; /* Must go first. */
	int                p_exit;
	pthread_t          p_thread;
	pthread_mutex_t    p_lock;
	pthread_cond_t     p_todo;
	int                p_nr_ready;
	int                p_nr_wait;
	struct rr_thread  *p_run;
	struct rr_thread **p_ready;
	struct rr_thread **p_wait;
};

static int nr_processors;
static int nr_threads;
static int nr_t = 0;
static struct processor *procs;

enum state { WAIT, READY, RUN };

static void  proc_init(struct processor *p);
static void  proc_fini(struct processor *p);
static void  proc_lock(struct processor *p);
static void  proc_unlock(struct processor *p);
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

int rr_init(int proc_nr, int thread_nr)
{
	int i;

	nr_processors = proc_nr;
	nr_threads = thread_nr;
	procs = calloc(proc_nr, sizeof procs[0]);
	assert(procs != NULL);
	for (i = 0; i < nr_processors; ++i) {
		proc_init(&procs[i]);
	}
	return 0;
}

void rr_fini(void)
{
	int i;
	for (i = 0; i < nr_processors; ++i) {
		proc_fini(&procs[i]);
	}
	free(procs);
}

int rr_start(void)
{
	int i;
	for (i = 0; i < nr_processors; ++i) {
		pthread_create(&procs[i].p_thread, NULL, &proc, &procs[i]);
	}
	return 0;
}

struct rr_thread *rr_thread_init(void (*f)(void *), void *arg)
{
	struct rr_thread  *t     = calloc(1, sizeof *t);
	int chunk = nr_threads / nr_processors;
	if (chunk < 128)
		chunk = 128;
	if (t != NULL) {
		struct processor *proc = &procs[(nr_t++ / chunk) % nr_processors];
		ustack_init(&t->r_stack, &proc->p_sched, f, arg, NULL, 0);
		pthread_mutex_lock(&proc->p_lock);
		assert(proc->p_nr_ready < nr_threads);
		proc->p_ready[proc->p_nr_ready] = t;
		if (proc->p_nr_ready++ == 0)
			pthread_cond_signal(&proc->p_todo);
		pthread_mutex_unlock(&proc->p_lock);
	}
	return t;
}

void rr_wait(void)
{
	struct ustack    *u = ustack_self();
	struct rr_thread *t = (void *)u;
	struct processor *p = (void *)u->u_sched;
	proc_lock(p);
	assert(t == p->p_run);
	if (t->r_nr_wake == 0) {
		assert(p->p_nr_wait < nr_threads);
		t->r_idx = p->p_nr_wait;
		p->p_wait[p->p_nr_wait++] = t;
		p->p_run = NULL;
		proc_unlock(p);
		ustack_block();
	} else {
		t->r_nr_wake--;
		proc_unlock(p);
	}
}

void rr_done(void)
{
	free(ustack_self()->u_stack);
}

void rr_wake(struct rr_thread *t)
{
	struct processor *p = (void *)t->r_stack.u_sched;
	proc_lock(p);
	if (rr_state(t) == WAIT) {
		assert(p->p_nr_ready < nr_threads);
		--p->p_nr_wait;
		p->p_wait[t->r_idx] = p->p_wait[p->p_nr_wait];
		p->p_wait[t->r_idx]->r_idx = t->r_idx;
		p->p_ready[p->p_nr_ready] = t;
		if (p->p_nr_ready++ == 0)
			pthread_cond_signal(&p->p_todo);
	} else
		t->r_nr_wake++;
	proc_unlock(p);
}

struct ustack *rr_next(struct usched *s)
{
	struct processor *p = (void *)s;
	proc_lock(p);
	while (p->p_nr_ready == 0) {
		if (p->p_exit && p->p_nr_wait == 0) {
                	proc_unlock(p);
                	return NULL;
        	}
		pthread_cond_wait(&p->p_todo, &p->p_lock);
	}
	p->p_run = p->p_ready[--p->p_nr_ready];
	proc_unlock(p);
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
	struct rr_thread **wait  = calloc(nr_threads, sizeof wait[0]);
	struct rr_thread **ready = calloc(nr_threads, sizeof ready[0]);
	assert(wait != NULL && ready != NULL);
	p->p_wait = wait;
	p->p_ready = ready;
	pthread_mutex_init(&p->p_lock, NULL);
	pthread_cond_init(&p->p_todo, NULL);
	p->p_sched.s_next  = &rr_next;
	p->p_sched.s_alloc = &rr_alloc;
	p->p_sched.s_free  = &rr_free;
}

static void proc_fini(struct processor *p)
{
	proc_lock(p);
	p->p_exit = 1;
	pthread_cond_signal(&p->p_todo);
	proc_unlock(p);
	pthread_join(p->p_thread, NULL);
	pthread_cond_destroy(&p->p_todo);
	pthread_mutex_destroy(&p->p_lock);
	free(p->p_wait);
	free(p->p_ready);
}

static void *proc(void *arg)
{
	struct processor *p = arg;
	usched_run(&p->p_sched);
	return NULL;
}

#if defined(SINGLE_THREAD)
static void  proc_lock(struct processor *p)
{
}

static void  proc_unlock(struct processor *p)
{
}
#else
static void  proc_lock(struct processor *p)
{
	pthread_mutex_lock(&p->p_lock);
}

static void  proc_unlock(struct processor *p)
{
	pthread_mutex_unlock(&p->p_lock);
}
#endif
