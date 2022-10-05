/* gcc -O -fcoroutines -Wall coro.cpp -lcppcoro -lstdc++ -pthread  */
/* https://github.com/andreasbuhr/cppcoro.git */
#include <cppcoro/shared_task.hpp>
#include <cppcoro/single_consumer_event.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <iostream>
#include <chrono>

static int n;
static int r;
static int m;
static int d;
static uint32_t p;

static std::vector<cppcoro::shared_task<void>>  t;
static cppcoro::single_consumer_event          *e;
static cppcoro::static_thread_pool             *tp;

static cppcoro::shared_task<void> pingpong(int idx)
{
	auto next = idx / n * n + (idx + 1) % n;
	if (p > 1) {
		co_await tp->schedule();
	}
	for (int i = 0; i < m; ++i) {
		if (idx % n == i % n) {
			// std::cout << i << " * " << idx << " send\n";
			e[next].set();
			// std::cout << i << " * " << idx << " wait\n";
			co_await e[idx];
		} else {
			// std::cout << i << "   " << idx << " wait\n";
			co_await e[idx];
			// std::cout << i << "   " << idx << " send\n";
			e[next].set();
		}
	}
}

static cppcoro::shared_task<int> work()
{
	co_await cppcoro::when_all(t);
	co_return 0;
}

using namespace std::chrono;

int main(int argc, char **argv)
{
	n = std::atoi(argv[1]); /* Cycle length. */
	r = std::atoi(argv[2]); /* Number of cycles. */
	m = std::atoi(argv[3]); /* Number of rounds. */
	d = std::atoi(argv[4]); /* Additional stack depth. */
	p = std::atoi(argv[5]); /* Number of processors. */
	cppcoro::static_thread_pool pool{p};
	tp = &pool;

	e = new cppcoro::single_consumer_event[n * r];
	for (int i = 0; i < n * r; ++i) {
		t.push_back(pingpong(i));
	}

	auto t0 = system_clock::now();
	cppcoro::sync_wait(work());
	auto t1 = system_clock::now();
	printf("%f\n", duration_cast<nanoseconds>(t1 - t0).count() / 1000000000.);
	delete[] e;
}
