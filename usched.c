/*
 * Overview
 * --------
 *
 * A simple dispatcher for cooperative user-space threads.
 *
 * A typical implementation of user-space threads allocates a separate stack for
 * each thread when the thread is created and then dispatches threads (as
 * decided by the scheduler) through some context switching mechanism, for
 * example, longjmp().
 *
 * In usched all threads (represented by struct ustack) are executed on the same
 * "native" stack. When a thread is about to block (usched_block()), a memory
 * buffer for the stack used by this thread is allocated and the stack is copied
 * to the buffer. After that the part of the stack used by the blocking thread
 * is discarded (by longjmp()-ing to the base of the stack) and a new thread is
 * selected. The stack of the selected thread is restored from its buffer and
 * the thread is resumed by longjmp()-ing to the usched_block() that blocked it.
 *
 * The focus of this implementation is simplicity: the total size of usched.[ch]
 * is less than 120LOC, as measured by SLOCCount.
 *
 * Advantages:
 *
 *     - no need to allocate maximal possible stack at thread initialisation:
 *       stack buffer is allocated as needed. It is also possible to free the
 *       buffer when the thread is resumed (not currently implemented);
 *
 *     - thread that doesn't block has 0 overhead: it is executed as a native
 *       function call (through a function pointer) without any context
 *       switching;
 *
 *     - because the threads are executed on the stack of the same native
 *       underlying thread, native synchronisation primitives (mutices, etc.)
 *       work, although the threads share underlying TLS. Of course one cannot
 *       use native primitives to synchronise between usched threads running on
 *       the same native thread.
 *
 * Disadvantages:
 *
 *     - stack copying introduces overhead (memcpy()) in each context switch;
 *
 *     - because stacks are moved around, addresses on a thread stack are only
 *       valid while the thread is running. This invalidates certain common
 *       programming idioms: other threads and heap cannot store pointers to the
 *       stacks, at least to the stacks of the blocked threads. Note that Go
 *       language, and probably other run-times, maintains a similar invariant.
 *
 * Usage
 * -----
 *
 * usched is only a dispatcher and not a scheduler: it blocks and resumes
 * threads but
 *
 *     - it does not keep track of threads (specifically allocation and freeing
 *       of struct ustack instances is done elsewhere),
 *
 *     - it implements no scheduling policies.
 *
 * These things are left to the user, together with stack buffers allocation and
 * freeing. The user supplies 3 call-backs:
 *
 *     - usched::s_next(): the scheduling function. This call-backs returns the
 *       next thread to execute. This can be either a new (never before
 *       executed) thread initialised with ustack_init(), or it can be a blocked
 *       thread. The user must keep track of blocked and runnable threads,
 *       presumably by providing wrappers to ustack_init() and ustack_block()
 *       that would record thread state changes. It is up to usched::s_next() to
 *       block and wait for events if there are no runnable threads and all
 *       threads are waiting for something. If usched::s_next() returns NULL,
 *       the dispatcher loop usched_run() exits. It is in general unsafe to
 *       re-start it again (unless you can guarantee that it will be restarted
 *       with exactly the same stack pointer, see the anchor assertion in
 *       usched_run()) so usched::s_next() should return NULL only when it is
 *       time to shutdown;
 *
 *     - usched::s_alloc(): allocates new stack buffer of at least the specified
 *       size. The user have full control over stack buffer allocation. It is
 *       possible to pre-allocate the buffer when the thread is initialised
 *       (reducing the cost of usched_block()), it is possible to cache buffers,
 *       etc.;
 *
 *     - usched::s_free(): frees the previously allocated stack buffer.
 *
 * An implementation of these call-backs must satisfy the following
 * requirements:
 *
 *     - usched::s_next() returns only new or blocked threads. That is, if a
 *       thread completed by returning from its startup function ustack::u_f()
 *       or aborted by calling usched_abort(), usched::s_next() never returns it
 *       again;
 *
 *     - usched::s_alloc() always returns with the allocated stack buffer. That
 *       is, usched::s_alloc() has the following options if it fails to allocate
 *       the buffer of the specified size:
 *
 *           + do some cleanup and then abort the current thread by calling
 *             ustack_abort(), which longjmp-s to usched_run(). The aborted
 *             thread must never be returned by usched::s_next();
 *
 *           + do some cleanup and then longjmp to some point within the thread
 *             stack, so that the thread continues the execution (and may re-try
 *             blocking later again);
 *
 *           + longjmp out of usched_run() completely or kill the current native
 *             thread.
 *
 * After installing the above call-backs in an instance of struct usched, the
 * user calls usched_run() either from the main thread or form a dedicated
 * native thread.
 *
 * rr.[ch] provides a simple "round-robin" scheduler implementing all the
 * call-backs. Use it carefully, it was only tested with rmain.c benchmark.
 *
 * Pictures!
 * ---------
 *
 * The following diagrams show stack management by usched. The stack grows from
 * right to left.
 *
 * At the entrance to the dispatcher loop. usched_run(S):
 *
 *                                                    usched_run()
 *    ----------------------------------------------+--------------+-------+
 *                                                  | buf | anchor |  ...  |
 *    ----------------------------------------------+--------------+-------+
 *                                                  ^
 *                                                  |
 *                                                  sp = S->s_buf
 *
 * A new (never before executed) thread U is selected by S->s_next(), launch()
 * calls the thread startup function U->u_f():
 *
 *                                   U->u_f() launch() usched_run()
 *     -----------------------------+---------+-----+--------------+-------+
 *                                  |         | pad | buf | anchor |  ...  |
 *     -----------------------------+---------+-----+--------------+-------+
 *                                  ^         ^
 *                                  |         |
 *                                  sp        U->u_bottom
 *
 * Thread executes as usual on the stack, until it blocks by calling
 * usched_block():
 *
 *
 *        usched_block()       bar() U->u_f() launch() usched_run()
 *     ----------+------+-----+-----+---------+-----+--------------+-------+
 *               | here | ... |     |         | pad | buf | anchor |  ...  |
 *     ----------+------+-----+-----+---------+-----+--------------+-------+
 *          ^    ^                            ^
 *          |    +-- sp = U->u_cont           |
 *          |                                 U->u_bottom
 *          U->u_top
 *
 * The stack from U->u_top to U->u_bottom is copied into the stack buffer
 * U->u_stack, and control returns to usched_run() by longjmp(S->s_buf):
 *
 *                                                    usched_run()
 *    ----------------------------------------------+--------------+-------+
 *                                                  | buf | anchor |  ...  |
 *    ----------------------------------------------+--------------+-------+
 *                                                  ^
 *                                                  |
 *                                                  sp = S->s_buf
 *
 * Next, suppose S->s_next() selects a previously blocked thread V ready to be
 * resumed. usched_run() calls cont(V).
 *
 *                                            cont()  usched_run()
 *    ----------------------------------------+-----+--------------+-------+
 *                                            |     | buf | anchor |  ...  |
 *    ----------------------------------------+-----+--------------+-------+
 *                                            ^
 *                                            |
 *                                            sp
 *
 * cont() copies the stack from the buffer to [V->u_top, V->u_bottom]
 * range. It's important that this memcpy() operation does not overwrite
 * cont()'s own stack frame, this is why pad[] array is needed in launch(): it
 * advances V->u_bottom and gives cont() some space to operate.
 *
 *      usched_block()       foo() V->u_f()   cont()  usched_run()
 *    ---------+------+-----+-----+--------+--+-----+--------------+-------+
 *             | here | ... |     |        |  |     | buf | anchor |  ...  |
 *    ---------+------+-----+-----+--------+--+-----+--------------+-------+
 *        ^    ^                           ^  ^
 *        |    +-- V->u_cont               |  +-- sp
 *        |                                |
 *        V->u_top                         V->u_bottom
 *
 * Then cont() longjmp()-s to V->u_cont, restoring V execution context:
 *
 *      usched_block()       foo() V->u_f()   cont()  usched_run()
 *    ---------+------+-----+-----+--------+--+-----+--------------+-------+
 *             | here | ... |     |        |  |     | buf | anchor |  ...  |
 *    ---------+------+-----+-----+--------+--+-----+--------------+-------+
 *             ^
 *             +-- sp = V->u_cont
 *
 * V continues its execution as if it returned from usched_block().
 *
 * Multiprocessing
 * ---------------
 *
 * By design, a single instance of struct usched cannot take advantage of
 * multiple processors, because all its threads are executed within a single
 * native thread. Multiple instances of struct usched can co-exist within a
 * single process address space, but a ustack thread created for one instance
 * cannot be migrated to another. One possible strategy to add support for
 * multiple processors is to create multiple instances of struct usched and
 * schedule them (that is, schedule the threads running respective
 * usched_run()-s) to processors via pthread_setaffinity_np() or similar.
 *
 * Current limitations
 * -------------------
 *
 *  - the stack is assumed to grow toward lower addresses. This is easy to fix,
 *    if necessary;
 *
 *  - the implementation is not signal-safe. Fixing this can be as easy as
 *    replacing *jmp() calls with their sig*jmp() counterparts. At the moment
 *    signal-based code, like gperf -lprofiler library, would most likely crash
 *    usched;
 *
 *  - usched.c must be compiled without optimisations and with
 *    -fno-stack-protector option (gcc);
 *
 *  - usched threads are cooperative: a thread will continue to run until it
 *    completes of blocks. Adding preemption (via signal-based timers) is
 *    relatively easy, the actual preemption decision will be relegated to the
 *    external "scheduler" via a new usched::s_preempt() call-back invoked from
 *    a signal handler.
 *
 */
/* cc -fno-stack-protector */
#include "usched.h"
#include <setjmp.h>
#include <stdlib.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

static void launch(struct ustack *u);
static void cont  (struct ustack *u);

static void stack_in (struct ustack *u);
static void stack_out(struct ustack *u);

static __thread struct ustack *current = NULL;

void usched_run(struct usched *s)
{
	int            anchor;
	jmp_buf        buf;
	struct ustack *u;
	assert(s->s_anchor == NULL || s->s_anchor == &anchor);
	assert(s->s_next  != NULL);
	assert(s->s_alloc != NULL);
	assert(s->s_free  != NULL);
	s->s_anchor = &anchor;
	setjmp(buf);
	s->s_buf = &buf;
	while ((u = s->s_next(s)) != NULL) {
		current = u;
		u->u_bottom != NULL ? cont(u) : launch(u);
		current = NULL;
	}
}

void ustack_init (struct ustack *u, struct usched *s,
		  void (*f)(void *), void *arg,
		  void *stack, int len)
{
	*u = (struct ustack) {
		.u_sched = s,
		.u_stack = stack,
		.u_len   = len,
		.u_f     = f,
		.u_arg   = arg
	};
}

void ustack_block(void)
{
	jmp_buf here;
	assert((void *)&here < current->u_bottom);
	if (setjmp(here) == 0) {
		current->u_cont = &here;
		current->u_top = current->u_cont - 32;
		stack_out(current);
		longjmp(*(jmp_buf *)current->u_sched->s_buf, 1);
	}
}

void ustack_abort(void)
{
	longjmp(*(jmp_buf *)current->u_sched->s_buf, 1);
}

enum { PAD = 300 };
static void launch(struct ustack *u)
{
	char pad[PAD]; /* Prevents stack_in() from smashing cont() frame. */
	u->u_bottom = pad;
	(*u->u_f)(u->u_arg);
}

static void cont(struct ustack *u)
{
	stack_in(u);
	longjmp(*(jmp_buf *)u->u_cont, 1);
}

static void stack_in(struct ustack *u)
{
	memcpy(u->u_top, u->u_stack, u->u_bottom - u->u_top);
}

static void stack_out(struct ustack *u)
{
	struct usched *s    = u->u_sched;
	int            used = u->u_bottom - u->u_top;

	if (u->u_stack != NULL && u->u_len < used) {
		s->s_free(s, u->u_stack, u->u_len);
		u->u_stack = NULL;
	}
	if (u->u_stack == NULL) {
		u->u_stack = s->s_alloc(s, used);
		u->u_len = used;
		assert(u->u_stack != NULL);
	}
	memcpy(u->u_stack, u->u_top, used);
}

struct ustack *ustack_self()
{
	assert(current != NULL);
	return current;
}
