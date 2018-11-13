#pragma once

#include <cstdint>
#include <type_traits>
#include <array>
#include <thread>
#include <atomic>

#define ALIGNMENT 64

template<typename T>
class alignas(ALIGNMENT) MobileAtomic {
public:
  MobileAtomic() {}  
  MobileAtomic(const MobileAtomic& a) : v(a.v.load()) {}
  
  MobileAtomic& operator=(const T& x) { v.store(x); return *this; }
  operator T() {return v.load();}

private:
  std::atomic<T> v;
};

extern "C" {

struct alignas(ALIGNMENT) ThreadBuffer {
// keep this fixed, as LLVM checks
  bool keep_going = true;
  bool start_prologue = false;
  uint64_t iteration_to_run;
  ThreadBuffer *next;
  
  // can add more stuff here
  ThreadBuffer(int t);
  int tid;
  std::thread runner;
  std::array<MobileAtomic<bool>, 16> flags;
};
using optim_type = std::add_pointer<void(ThreadBuffer*,void*)>::type;
using opaque_ptr = std::add_pointer<int8_t>::type;

void thread_signal(ThreadBuffer * next, uint8_t s);
void thread_wait(ThreadBuffer * current, uint8_t s);

void schedule_helix(optim_type o, opaque_ptr carry, int num_threads);
void stop_helix(ThreadBuffer* tb);

void print_abc(int a, int b, int c);
}

int heavy_function(int x);

void prevent_deletion() {
	ThreadBuffer x(1);
}
