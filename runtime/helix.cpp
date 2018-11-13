#include "helix.h"

#include <cstdio>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <mutex>

std::mutex print_guard;
std::unordered_map<std::thread::id, int> thread_ids;

ThreadBuffer::ThreadBuffer(int t) : tid(t) {
	for(auto& f : flags) {
		f = false;
	}  
  }

void thread_signal(ThreadBuffer * next, uint8_t s) {
	{
		std::lock_guard<std::mutex> m{print_guard};
		std::cout << "thread_signal next_tid=" << next->tid << " s=" << (int)s << " addr=" << (void*)&(next->flags[s]) << std::endl;
	}
	next->flags[s] = true;
}

void thread_wait(ThreadBuffer * current, uint8_t s) {
	{
		std::lock_guard<std::mutex> m{print_guard};
		std::cout << "thread_wait tid=" << current->tid << " s=" << (int)s << " addr=" << (void*)&(current->flags[s])<< std::endl;
	}
	while(current->flags[s] == false) {}
	current->flags[s] = false;

	{
		std::lock_guard<std::mutex> m{print_guard};
		std::cout << "thread_wait tid=" << current->tid << " s=" << (int)s << " finished" << " addr=" << (void*)&(current->flags[s]) << std::endl;
	}
}

void schedule_helix(optim_type o, opaque_ptr carry, int num_threads) {
	
	std::cout << "size of thread buffer=" << sizeof(ThreadBuffer) << std::endl;
	
	std::vector<ThreadBuffer> tbs;
	
	{
		std::lock_guard<std::mutex> m{print_guard};
		for(int i = 0; i < num_threads; i++) {
			tbs.push_back(ThreadBuffer(i));
		}
		
		thread_ids[std::this_thread::get_id()] = 0;	
		std::cout << "scheduling helix from thread" << std::this_thread::get_id() << std::endl;
			
		for(int i = 0; i < num_threads; i++) {
			auto& tb = tbs[i];
			tb.next = &tbs[(i+1)%num_threads];
			tb.keep_going = true;
			if(i > 0) {
				tb.runner = std::thread(o, &tb, carry);
				thread_ids[tb.runner.get_id()] = i;
				std::cout << "created extra thread with id=" << thread_ids[tb.runner.get_id()] << " " << i << std::endl;
			}
		}
	}
	
	
	auto& tb = tbs[0];
	tb.start_prologue = true;
	for(auto& f : tb.flags) {
		f = true;
	}
	o(&tb, carry);

	printf("joining the threads that we just spawned\n");
	for(int i = 1; i < num_threads; i++) {
		auto& tb = tbs[i];
		tb.runner.join();
	}
}

void stop_helix(ThreadBuffer* tb) {
	std::lock_guard<std::mutex> m{print_guard};
	std::cout << "stopping helix from thread" << thread_ids[std::this_thread::get_id()] << std::endl;
	
	while(tb->keep_going) {
		std::cout << "stopping helix for thread i=" << tb->tid << " OS id=" << tb->runner.get_id() << std::endl;
		
		//__atomic_store_1();
		//std::_Atomic_store_1(static_cast<unsigned char*>(&(tb->keep_going)), false, 5);
		tb->keep_going = false;
		tb = tb->next;
	}
}

void print_abc(int a, int b, int c) {
	
	{
	std::lock_guard<std::mutex> m{print_guard}; 
	std::cout << "Thread=" << thread_ids[std::this_thread::get_id()] << " a=" << a 
													 << " b=" << b 
													 << " c=" << c 
													 << std::endl;
	}

	//std::this_thread::sleep_for(std::chrono::milliseconds(100));
 }
 
 [[clang::optnone]] int heavy_function(int x) {
	std::this_thread::sleep_for(std::chrono::seconds(1));
	return 1000;
}
 
 /*
#include <thread>

struct ThreadStuff {
  bool keep_going = true;
  bool start_prologue = false;
  uint64_t iteration_to_run;

  bool prologue(uint64_t i) { return i < 100; }
  void stop() {}
  void body(uint64_t i) {}
  ThreadStuff *next;
};


void test_worker_thread(ThreadStuff *h, const int idx) {
  while (true) {
    if (h->keep_going == false) {
      break;
    }

    if (h->start_prologue == false) {
      if (h->keep_going == false) {
        break;
      }
      continue;
    }
    h->start_prologue = false;

    const uint64_t current_iteration = h->iteration_to_run;
    bool res = h->prologue(current_iteration);
    if (!res) {
      h->stop();
      break;
    }
    h->next->iteration_to_run = current_iteration + 1;
    h->next->start_prologue = true;
    h->body(current_iteration);
  }
};
*/

/*
1. get original loop value lists
2. get loop carried dependencies from original loop
3. create new function based on the loop carried dependencies that are found
4. copy original loop into separate function based on the loop value lists
	- patch the phi nodes
	- patch the jumps (including jump to start_next and end etc)
5. translate the loop carried dependencies into the new loop
*/

/*
1. ensure that anything needed to decide the prologue is only written to in the prologue
	(it can be read, but only read the register value, not the loaded value). 
	These values will be synced on the prologue atomic and have to be available for the next prologe.
2. for all the other loop carried dependencies, find the first read R and the last write W.
	in case W > R in program order, insert a barrier before the first read and after the last write
3. relax the synchronization ranges - try to move as much instructions before/after them (?)
*/