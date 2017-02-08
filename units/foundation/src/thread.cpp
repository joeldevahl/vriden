#include <foundation/thread.h>
#include <foundation/assert.h>
#include <foundation/allocator.h>

#if defined(FAMILY_WINDOWS)

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>

thread_t* thread_create(const char * name, thread_main_func_t func, void * data)
{
	(void)name;
	return (thread_t*)CreateThread(0x0, 0, (LPTHREAD_START_ROUTINE)func, data, 0, 0x0);
}

void thread_destroy(thread_t* thread)
{
	CloseHandle(thread);
}

void thread_join(thread_t* thread)
{
	WaitForSingleObject(thread, INFINITE);
}

void thread_exit()
{
	ExitThread(0x0);
}

void thread_yield()
{
	SwitchToThread();
}

void thread_sleep(uint32_t milliseconds)
{
	Sleep(milliseconds);
}

uint64_t thread_get_current_id()
{
	return GetCurrentThreadId();
}

#elif defined(FAMILY_UNIX)

#include <pthread.h>
#include <unistd.h>

union thread_converter
{
	thread_t* thread;
	pthread_t pthread;
};

typedef void *(*pthread_start_func_t)(void*);

thread_t* thread_create(const char * name, thread_main_func_t func, void * data)
{
	union thread_converter conv;

	STATIC_ASSERT(sizeof(thread_t*) <= sizeof(pthread_t));

	pthread_create(&conv.pthread, 0x0, (pthread_start_func_t)func, data);
	return conv.thread;
}

void thread_destroy(thread_t* thread)
{
	union thread_converter conv;
	conv.thread = thread;
	pthread_detach(conv.pthread);
}

void thread_join(thread_t* thread)
{
	union thread_converter conv;
	conv.thread = thread;
	pthread_join(conv.pthread, 0x0);
}

void thread_exit()
{
	pthread_exit(0x0);
}

void thread_yield()
{
	sched_yield();
}

void thread_sleep(uint32_t milliseconds)
{
	usleep(milliseconds * 1000);
}

uint64_t thread_get_current_id()
{
	uint64_t tid;
	pthread_threadid_np(NULL, &tid);
	return tid;
}

#else
#	error Not implemented for this platform
#endif
