/* Minimal dispatcher based on stack copying. See usched.c for details. */

/* Dispatcher instance. */
struct usched {
	/* Anchor. Used to check that if usched_run() is called again on the
	   same dispatcher, stack pointer is the same. */
	void            *s_anchor;
	/* Pointer to jmpbuf local to usched_run() frame. */
	void            *s_buf;
	/* User-provided scheduler. */
	struct ustack *(*s_next )(struct usched *);
	/* User-provided call-back to allocate new stack buffer. */
	void          *(*s_alloc)(struct usched *, int);
	/* User-provided call-back to free previously allocated stack buffer. */
	void           (*s_free )(struct usched *, void *, int);
};

/* "Thread" managed by the dispatcher. */
struct ustack {
	struct usched *u_sched;
	void          *u_bottom; /* Outermost stack frame. */
	void          *u_top;    /* Innermost stack frame. */
	void          *u_stack;  /* Allocated stack buffer. */
	int            u_len;    /* Length of allocated stack buffer. */
	void          *u_cont;   /* Pointer to jmpbuf to resume the thread. */
	void         (*u_f)(void *arg); /* Startup function. */
	void          *u_arg;           /* Startup argument. */
};

/* Dispatches threads in a loop. Returns when ->s_next() returns NULL. */
void usched_run(struct usched *s);
/* Initialises a new thread. */
void ustack_init(struct ustack *u, struct usched *s, void (*f)(void *),
		 void *arg, void *stack, int len);
void ustack_block(void); /* Blocks currently running thread. */
void ustack_abort(void); /* Aborts currently run thread. */
struct ustack *ustack_self(void); /* Returns current thread. */
