#pragma once

#include <stdint.h>

struct thread_t;

typedef void (*thread_main_func_t)(void*);

thread_t* thread_create(const char * name, thread_main_func_t func, void * data);
void thread_destroy(thread_t* thread);
void thread_join(thread_t* thread);
void thread_exit();
void thread_yield();
void thread_sleep(uint32_t milliseconds);
uint32_t thread_get_current_id();
