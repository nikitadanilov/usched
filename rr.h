struct rr_thread;

int  rr_init (int proc_nr, int thread_nr);
int  rr_start(void);
void rr_fini (void);

struct rr_thread *rr_thread_init(void (*f)(void *), void *arg);
void rr_wait(void);
void rr_wake(struct rr_thread *t);
