#pragma once

#include <cstdint>
#include <type_traits>

#define ALIGNMENT 64

class ThreadBuffer;

extern "C"
{
  struct alignas(ALIGNMENT) ThreadBufferBase
  {
    // these values are read by LLVM upon code generation, do not change
    bool keep_going = true;
    bool start_prologue = false;
    uint64_t iteration_to_run;
    ThreadBuffer *next;
  };

  using optim_type = std::add_pointer<void(ThreadBuffer *, void *)>::type;
  using opaque_ptr = std::add_pointer<int8_t>::type;

  void thread_signal(ThreadBuffer *next, uint8_t s);
  void thread_wait(ThreadBuffer *current, uint8_t s);

  void schedule_helix(optim_type o, opaque_ptr carry, int num_threads);
  void stop_helix(ThreadBuffer *tb);

  void prevent_deletion()
  {
    ThreadBufferBase x;
  }
}
