  
#ifndef THREADS_CONF_H
#define THREADS_CONF_H

#include <atomic>
#include <functional>

// #include "user_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

int printf(const char* fmt, ...);

#if defined(__cplusplus)
}
#endif

#define THREAD_NUM   4

extern std::function<void()> fun_ptrs[THREAD_NUM];

void task_notify(int threads);
void task_wait_done(int threads);

#endif