#include "helix.h"

#include <cstdio>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cassert>
#include <array>
#include <thread>
#include <atomic>

const bool print_debug = std::getenv("HELIX_DEBUG") != nullptr;
std::mutex print_guard;

struct alignas(ALIGNMENT) ThreadBuffer : public ThreadBufferBase
{
	// can add more stuff here
	ThreadBuffer()
	{
		for (auto &f : flags)
		{
			f = false;
		}
	}
	int tid;
	std::thread runner;
	std::array<std::atomic<bool>, 16> flags;
};

void thread_signal(ThreadBuffer *next, uint8_t s)
{
	if (print_debug)
	{
		std::lock_guard<std::mutex> m{print_guard};
		std::cout << "thread_signal next_tid=" << next->tid << " s=" << (int)s << " addr=" << (void *)&(next->flags[s]) << std::endl;
	}
	next->flags[s] = true;
}

void thread_wait(ThreadBuffer *current, uint8_t s)
{
	assert(current->runner.get_id() == std::this_thread::get_id());

	if (print_debug)
	{
		std::lock_guard<std::mutex> m{print_guard};
		std::cout << "thread_wait tid=" << current->tid << " s=" << (int)s << " addr=" << (void *)&(current->flags[s]) << std::endl;
	}
	while (current->flags[s] == false)
	{
	}
	current->flags[s] = false;

	if (print_debug)
	{
		std::lock_guard<std::mutex> m{print_guard};
		std::cout << "thread_wait tid=" << current->tid << " s=" << (int)s << " finished"
				  << " addr=" << (void *)&(current->flags[s]) << std::endl;
	}
}

void schedule_helix(optim_type o, opaque_ptr carry, int num_threads)
{
	std::vector<ThreadBuffer> tbs{static_cast<size_t>(num_threads)};

	if (print_debug)
	{
		std::lock_guard<std::mutex> m{print_guard};
		std::cout << "scheduling helix from thread" << std::this_thread::get_id() << std::endl;
	}

	for (int i = 0; i < num_threads; i++)
	{
		auto &tb = tbs[i];
		tb.next = &tbs[(i + 1) % num_threads];
		tb.keep_going = true;
		tb.tid = i;

		if (i > 0)
		{
			tb.runner = std::thread(o, &tb, carry);
			if (print_debug)
			{
				std::lock_guard<std::mutex> m{print_guard};
				std::cout << "created extra thread with id=" << i << std::endl;
			}
		}
	}

	auto &tb = tbs[0];
	tb.start_prologue = true;
	for (auto &f : tb.flags)
	{
		f = true;
	}
	o(&tb, carry);

	if (print_debug)
	{
		std::lock_guard<std::mutex> m{print_guard};
		std::cout << "joining the threads that we just spawned" << std::endl;
	}

	for (int i = 1; i < num_threads; i++)
	{
		auto &tb = tbs[i];
		tb.runner.join();
	}
}

void stop_helix(ThreadBuffer *tb)
{
	assert(tb->runner.get_id() == std::this_thread::get_id());

	if (print_debug)
	{
		std::lock_guard<std::mutex> m{print_guard};
		std::cout << "stopping helix from thread" << tb->runner.get_id() << " " << tb->tid << std::endl;
	}

	while (tb->keep_going)
	{
		if (print_debug)
		{
			std::lock_guard<std::mutex> m{print_guard};
			std::cout << "stopping helix for thread i=" << tb->tid << " OS id=" << tb->runner.get_id() << std::endl;
		}

		tb->keep_going = false;
		tb = tb->next;
	}
}
