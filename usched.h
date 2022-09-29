struct usched {
	void            *s_anchor;
	void            *s_buf;
	struct ustack *(*s_next )(struct usched *);
	void          *(*s_alloc)(struct usched *, int);
	void           (*s_free )(struct usched *, void *, int);
};

struct ustack {
	struct usched *u_sched;
	void          *u_bottom;
	void          *u_top;
	void          *u_stack;
	int            u_len;
	void          *u_cont;
	void         (*u_f)(void *arg);
	void          *u_arg;
};

int  usched_init(struct usched *s);
void usched_fini(struct usched *s);
void usched_run(struct usched *s);
void ustack_init(struct ustack *u, struct usched *s, void (*f)(void *),
		 void *arg, void *stack, int len);
void ustack_block(void);
void ustack_abort(void);
struct ustack *ustack_self(void);
