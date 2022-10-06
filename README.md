# usched: stackswap coroutines

This repository contains a simple experimental implementation of
[coroutines](https://en.wikipedia.org/wiki/Coroutine) alternative to well-known
"stackless" and "stackful" methods.

The term "coroutine" gradually grew to mean a mechanism where a computation,
which in this context means a chain of nested function calls, can "block" or
"yield" so that the top-most caller can proceed and the computation can later be
resumed at the blocking point with the chain of intermediate function activation
frames preserved.

Prototypical uses of coroutines are lightweight support for potentially blocking
operations (user interaction, IO, networking) and
[*generators*](https://en.wikipedia.org/wiki/Generator_(computer_programming)),
which produce multiple values (see [same fringe
problem](https://wiki.c2.com/?SameFringeProblem)).

There are two common coroutine implementation methods:

- a *stackful* coroutine runs on a separate stack. When a stackful coroutine
  blocks, it performs a usual context switch. Historically "coroutines" meant
  stackful coroutines. Stackful coroutines are basically little more than usual
  threads, and so they can be *kernel* (supported by the operating system) or
  *user-space* (implemented by a user-space library, also known as *green
  threads*), preemptive or cooperative.
  
- a *stackless* coroutine does not use any stack when blocked. In a typical
  implementation instead of using a normal function activation frame on the
  stack, the coroutine uses a special activation frame allocated in the heap so
  that it can outlive its caller. Using heap-allocated frame to store all local
  variable lends itself naturally to compiler support, but some people are known
  to implement stackless coroutines manually via a combination of
  pre-processing, library and tricks much worse than [Duff's
  device](https://en.wikipedia.org/wiki/Protothread).
  
Stackful and stateless are by no means the only possibilities. One of the
earliest languages to feature generators
[CLU](https://en.wikipedia.org/wiki/CLU_(programming_language))
([distribution](https://pmg.csail.mit.edu/ftp.lcs.mit.edu/pub/pclu/CLU/)) ran
generators on the caller's stack.

usched is in some sense intermediate between stackful and stackless: its
coroutines do not use stack when blocked, nor do they allocate individual
activation frames in the heap.

The following is copied with some abbreviations from
[usched.c](https://github.com/nikitadanilov/usched/blob/master/usched.c).

Overview
--------

usched: A simple dispatcher for cooperative user-space threads.

A typical implementation of user-space threads allocates a separate stack for
each thread when the thread is created and then dispatches threads (as
decided by the scheduler) through some context switching mechanism, for
example, `longjmp()`.

In usched all threads (represented by struct ustack) are executed on the same
"native" stack. When a thread is about to block (`usched_block()`), a memory
buffer for the stack used by this thread is allocated and the stack is copied
to the buffer. After that the part of the stack used by the blocking thread
is discarded (by `longjmp()`-ing to the base of the stack) and a new thread is
selected. The stack of the selected thread is restored from its buffer and
the thread is resumed by `longjmp()`-ing to the `usched_block()` that blocked it.

The focus of this implementation is simplicity: the total size of `usched.[ch]`
is less than 120LOC, as measured by SLOCCount.

Advantages:

- no need to allocate maximal possible stack at thread initialisation: stack
  buffer is allocated as needed. It is also possible to free the buffer when the
  thread is resumed (not currently implemented);

- thread that doesn't block has 0 overhead: it is executed as a native function
  call (through a function pointer) without any context switching;

- because the threads are executed on the stack of the same native underlying
  thread, native synchronisation primitives (mutices, etc.)  work, although the
  threads share underlying TLS. Of course one cannot use native primitives to
  synchronise between usched threads running on the same native thread.

Disadvantages:

- stack copying introduces overhead (`memcpy()`) in each context switch;

- because stacks are moved around, addresses on a thread stack are only valid
  while the thread is running. This invalidates certain common programming
  idioms: other threads and heap cannot store pointers to the stacks, at least
  to the stacks of the blocked threads. Note that Go language, and probably
  other run-times, maintains a similar invariant.

Usage
-----

usched is only a dispatcher and not a scheduler: it blocks and resumes
threads but

- it does not keep track of threads (specifically allocation and freeing of
  struct ustack instances is done elsewhere),

- it implements no scheduling policies.

These things are left to the user, together with stack buffers allocation and
freeing. The user supplies 3 call-backs:

- `usched::s_next()`: the scheduling function. This call-backs returns the next
  thread to execute. This can be either a new (never before executed) thread
  initialised with `ustack_init()`, or it can be a blocked thread. The user must
  keep track of blocked and runnable threads, presumably by providing wrappers
  to `ustack_init()` and `ustack_block()` that would record thread state changes. It
  is up to `usched::s_next()` to block and wait for events if there are no
  runnable threads and all threads are waiting for something;

- `usched::s_alloc()`: allocates new stack buffer of at least the specified
  size. The user have full control over stack buffer allocation. It is possible
  to pre-allocate the buffer when the thread is initialised (reducing the cost
  of `usched_block()`), it is possible to cache buffers, etc.;

- `usched::s_free()`: frees the previously allocated stack buffer.

[rr.h](https://github.com/nikitadanilov/usched/blob/master/rr.h) and
[rr.c](https://github.com/nikitadanilov/usched/blob/master/rr.c) provide a
simple "round-robin" scheduler implementing all the call-backs. Use it
carefully, it was only tested with
[rmain.c](https://github.com/nikitadanilov/usched/blob/master/rmain.c)
benchmark.

Pictures!
---------

The following diagrams show stack management by usched. The stack grows from
right to left.

At the entrance to the dispatcher loop. `usched_run(S)`:

                                                    usched_run()
    ----------------------------------------------+--------------+-------+
                                                  | buf | anchor |  ...  |
    ----------------------------------------------+--------------+-------+
                                                  ^
                                                  |
                                                  sp = S->s_buf

A new (never before executed) thread `U` is selected by `S->s_next()`, `launch()`
calls the thread startup function `U->u_f()`:

                                   U->u_f() launch() usched_run()
     -----------------------------+---------+-----+--------------+-------+
                                  |         | pad | buf | anchor |  ...  |
     -----------------------------+---------+-----+--------------+-------+
                                  ^         ^
                                  |         |
                                  sp        U->u_bottom

Thread executes as usual on the stack, until it blocks by calling
`usched_block()`:


        usched_block()       bar() U->u_f() launch() usched_run()
     ----------+------+-----+-----+---------+-----+--------------+-------+
               | here | ... |     |         | pad | buf | anchor |  ...  |
     ----------+------+-----+-----+---------+-----+--------------+-------+
          ^    ^                            ^
          |    +-- sp = U->u_cont           |
          |                                 U->u_bottom
          U->u_top

The stack from `U->u_top` to `U->u_bottom` is copied into the stack buffer
`U->u_stack`, and control returns to `usched_run()` by `longjmp(S->s_buf)`:

                                                    usched_run()
    ----------------------------------------------+--------------+-------+
                                                  | buf | anchor |  ...  |
    ----------------------------------------------+--------------+-------+
                                                  ^
                                                  |
                                                  sp = S->s_buf

Next, suppose `S->s_next()` selects a previously blocked thread `V` ready to be
resumed. `usched_run()` calls `cont(V)`.

                                            cont()  usched_run()
    ----------------------------------------+-----+--------------+-------+
                                            |     | buf | anchor |  ...  |
    ----------------------------------------+-----+--------------+-------+
                                            ^
                                            |
                                            sp

`cont()` copies the stack from the buffer to [`V->u_top`, `V->u_bottom`]
range. It's important that this `memcpy()` operation does not overwrite
`cont()`'s own stack frame, this is why `pad[]` array is needed in `launch()`: it
advances `V->u_bottom` and gives `cont()` some space to operate.

      usched_block()       foo() V->u_f()   cont()  usched_run()
    ---------+------+-----+-----+--------+--+-----+--------------+-------+
             | here | ... |     |        |  |     | buf | anchor |  ...  |
    ---------+------+-----+-----+--------+--+-----+--------------+-------+
        ^    ^                           ^  ^
        |    +-- V->u_cont               |  +-- sp
        |                                |
        V->u_top                         V->u_bottom

Then `cont()` `longjmp()`-s to `V->u_cont`, restoring `V` execution context:

      usched_block()       foo() V->u_f()   cont()  usched_run()
    ---------+------+-----+-----+--------+--+-----+--------------+-------+
             | here | ... |     |        |  |     | buf | anchor |  ...  |
    ---------+------+-----+-----+--------+--+-----+--------------+-------+
             ^
             +-- sp = V->u_cont

`V` continues its execution as if it returned from `usched_block()`.

Multiprocessing
---------------

By design, a single instance of `struct usched` cannot take advantage of
multiple processors, because all its threads are executing within a single
native thread. Multiple instances of `struct usched` can co-exist within a
single process address space, but a ustack thread created for one instance
cannot be migrated to another. One possible strategy to add support for multiple
processors is to create multiple instances of struct usched and schedule them
(that is, schedule the threads running respective `usched_run()`-s) to
processors via `pthread_setaffinity_np()` or similar. See
[rr.c](https://github.com/nikitadanilov/usched/blob/master/rr.c) for a
simplistic implementation.

Current limitations
-------------------

 - the stack is assumed to grow toward lower addresses. This is easy to fix,
   if necessary;

 - the implementation is not signal-safe. Fixing this can be as easy as
   replacing \*jmp() calls with their sig\*jmp() counterparts. At the moment
   signal-based code, like gperf -lprofiler library, would most likely crash
   usched;

 - `usched.c` must be compiled without optimisations and with
   `-fno-stack-protector` option (gcc);

 - usched threads are cooperative: a thread will continue to run until it
   completes of blocks. Adding preemption (via signal-based timers) is
   relatively easy, the actual preemption decision will be relegated to the
   external "scheduler" via a new `usched::s_preempt()` call-back invoked from
   a signal handler.

## Notes

[Midori](https://joeduffyblog.com/2015/11/19/asynchronous-everything/) seems to
use a similar method: a coroutine (called *activity* there) starts on the native
stack. If it needs to block, frames are allocated in the heap (this requires
compiler support) and filled in from the stack, the coroutine runs in these
heap-allocated frames when resumed.

## Benchmarks

usched was benchmarked against a few stackful (go, pthreads) and stackless
(rust, c++ coroutines) implementations. A couple of caveats:

- all benchmarking in general is subject to the reservations voiced by
  Hippocrates and usually translated (with the complete reversal of the meaning)
  as *ars longa, vita brevis*, which means: "the **art** [of doctor or tester]
  takes **long** time to learn, but the **life** of a student is **brief**,
  symptoms are vague, chances of success are doubtful".

- the author is much less than fluent with all the languages and frameworks used
  in the benchmarking. It is possible that some of the benchmarking code is
  inefficient or just outright wrong. Any comments are appreciated.
  
The benchmark tries to measure the efficiency of coroutine switching. It creates
R *cycles*, N coroutines each. Each cycle performs M *rounds*, where each round
consists of sending a message across the cycle: a particular coroutine (selected
depending on the round number) sends the message to its right neighbour, all
other coroutines relay the message received from the left to the right, the
round completes when the originator receives the message after it passed through
the entire cycle.

If N == 2, the benchmark is R pairs of processes, ping-ponging M messages within
each pair.

Some benchmarks support two additional parameters: D (additional space in bytes,
artificially consumed by each coroutine in its frame) and P (the number of
native threads used to schedule the coroutines.

The benchmark creates N\*R coroutines and sends a total of N\*R\*M messages,
the latter being proportional to the number of coroutine switches.

[bench.sh](https://github.com/nikitadanilov/usched/blob/master/bench.sh) runs
all implementations with the same N, R and M
parameters. [graph.py](https://github.com/nikitadanilov/usched/blob/master/graph.py)
plots the results.

- POSIX. Source:
  [pmain.c](https://github.com/nikitadanilov/usched/blob/master/pmain.c),
  binary: pmain. Pthreads-based stackful implementation in C. Uses default
  thread attributes. pmain.c also contains emulation of unnamed POSIX semaphores
  for Darwin. Plot label: "P". This benchmarks crashes with "pmain:
  pthread_create: Resource temporarily unavailable" for large values of N\*R\*M.

- Go. Source:
  [gmain.go](https://github.com/nikitadanilov/usched/blob/master/gmain.go),
  binary: gmain. The code is straightforward (it was a pleasure to write). D is
  supported via runtime.GOMAXPROCS(). "GO1T" are the results for a single native
  thread, "GO" are the results without the restriction on the number of threads.
  
- Rust. Source:
  [cycle/src/main.rs](https://github.com/nikitadanilov/usched/blob/master/cycle/src/main.rs),
  binary: cycle/target/release/cycle. Stackless implementation using Rust
  builtin [async/.await](https://rust-lang.github.io/async-book/). Label:
  "R". It is single-threaded (I haven't figured out how to distribute coroutines
  to multiple executors), so should be compared with GO1T, C++1T and
  U1T. Instead of fighting with the Rust borrow checker, I used "unsafe" and
  shared data-structures between multiple coroutines much like other benchmarks
  do.

- C++. Source:
  [c++main.cpp](https://github.com/nikitadanilov/usched/blob/master/c%2B%2Bmain.cpp),
  binary: c++main. The current state of coroutine support in C++ is unclear. Is
  everybody supposed to directly use `<coroutine>` interfaces or one of the
  mutually incompatible libraries that provide easier to use interfaces on top
  of `<coroutine>`? This benchmark uses [Lewis
  Baker](https://lewissbaker.github.io/)'s
  [cppcoro](https://github.com/lewissbaker/cppcoro), ([Andreas
  Buhr](https://www.andreasbuhr.de/)'s
  [fork](https://github.com/andreasbuhr/cppcoro)). Labels: "C++" and "C++1T" (for
  single-threaded results).
  
- usched. Source:
  [rmain.c](https://github.com/nikitadanilov/usched/blob/master/rmain.c),
  binary: rmain. Based on `usched.[ch]` and `rr.[ch]` Labels: "U", "U1K" (1000
  bytes of additional stack space for each coroutine), "U10K" (10000 bytes) and
  "U1T" (single-threaded).
  
[bench.sh](https://github.com/nikitadanilov/usched/blob/master/bench.sh) runs
all benchmarks with N == 2 (message ping-pong) and N == 8. Raw results are in
[results.linux](https://github.com/nikitadanilov/usched/blob/master/results.linux). In
the graphs, the horizontal axis is the number of coroutines (N\*R, logarithmic)
and the vertical axis is the operations (N\*R\*M) per second

Environment: Linux VM, 16 processors, 16GB of memory. Kernel: 4.18.0 (Rocky
Linux).

<a href="https://github.com/nikitadanilov/usched/blob/master/c8.png"><img src="https://github.com/nikitadanilov/usched/blob/master/c8.png"/></a>

(N == 8) A few notes:

- As mentioned above, pthreads-based solution crashes with around 50K threads.

- Most single-threaded versions ("GO1T", "R" and "U1T") are stable as corpse's
  body temperature. Rust cools off completely at about 50K
  coroutines. Single-threaded C++ ("C++1T") on the other hand is the most
  performant solution for almost the entire range of measurement, it is only for
  coroutine counts higher than 1M when "U" overtakes it.

- It is interesting that a very simple and unoptimised usched fares so well
  against heavily optimized C++ and Go run-times. (Again, see the reservations
  about the benchmarking.)
  
- Rust is disappointing.

<a href="https://github.com/nikitadanilov/usched/blob/master/c2.png"><img src="https://github.com/nikitadanilov/usched/blob/master/c2.png"/></a>

(N == 2)

- First, note that the scale is different on the vertical axis.

- Single-threaded benchmarks display roughly the same behaviour (exactly the
  same in "C++1T" case) as with N == 8.

- Go is somewhat better. Perhaps its scheduler is optimised for message
  ping-pong usual in channel-based concurrency models?
  
- usched variants are much worse (50% worse for "U") than N == 8.

- Rust is disappointing.

To reproduce:

    $ # install libraries and everything, then...
	$ make
	$ while : ;do ./bench.sh | tee -a results; sleep 5 ;done # collect enough results, this might take long...
	^C
	$ grep '^ *[2N],' results | python3 graph.py > c2.svg # create plot for N == 2
	$ grep '^ *[8N],' results | python3 graph.py > c8.svg # create plot for N == 8
