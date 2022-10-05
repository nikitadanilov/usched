/*
 * Overview
 * --------
 *
 * A simple dispatcher for cooperative user-space threads.
 *
 * A typical implementation of user-space threads allocates a separate stack for
 * each thread when the thread is created and then dispatches threads (as
 * decided by the scheduler) through something similar to longjmp().
 *
 * In usched all threads are executed on the same stack. When a thread wants to
 * block (usched_block()), a memory buffer for the stack used by this thread is
 * allocated and the stack is copied to the buffer. After that the part of the
 * stack used by the blocking thread is discarded (by longjmp()-ing to the base
 * of the stack) and a new thread is selected. The stack of the selected thread
 * is restored from its buffer and the thread is resumed by longjmp()-ing to the
 * usched_block() that blocked it.
 *
 * Advantages:
 *
 *     - no need to allocate maximal possible stack at thread initialisation:
 *       stack buffer is allocated as needed and can be freed when the thread is
 *       resumed (not currently implemented);
 *
 *     - thread that doesn't block has 0 overhead: it is executed as a native
 *       function call (through function pointer) without any context switching;
 *
 *     - because the threads are executed on the stack of same the native
 *       underlying thread, native synchronisation primitives (mutices, etc.)
 *       work, although the threads share underlying TLS.
 *
 * Disadvantages:
 *
 *     - stack copying introduced overhead (memcpy()) in each context switch;
 *
 *     - because stacks are moved around, addresses on a thread stack are only
 *       valid while the thread is running. This invalidates certain common
 *       programming idioms: other threads and heap cannot store pointers to the
 *       stacks, at least to the stacks of the blocked threads. Note that Go
 *       language, and probably other run-times, maintains a similar invariant.
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
 * Current limitations:
 *
 *     - the stack is assumed to grow toward lower addresses. This is easy to
 *       fix, if necessary.
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
