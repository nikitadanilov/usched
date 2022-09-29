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
