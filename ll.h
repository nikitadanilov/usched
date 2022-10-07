struct ll_thread;

int  ll_init (int proc_nr, int thread_nr);
int  ll_start(void);
void ll_fini (void);

struct ll_thread *ll_thread_init(void (*f)(void *), void *arg, int group);
void ll_wait(void);
void ll_wake(struct ll_thread *t);
void ll_done(void);
