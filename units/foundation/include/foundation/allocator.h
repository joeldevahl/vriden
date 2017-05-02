#pragma once

#include <new>
#include <stdint.h>
#include <stdarg.h>

#include "defines.h"

struct allocator_t;

typedef void* (*allocator_alloc_func_t)(allocator_t* allocator, size_t count, size_t size, size_t align, const char* file, int line);
typedef void* (*allocator_realloc_func_t)(allocator_t* allocator, void* memory, size_t count, size_t size, size_t align, const char* file, int line);
typedef void (*allocator_free_func_t)(allocator_t* allocator, void* memory, const char* file, int line);

/**
 * Helper struct to be used when passing around allocators.
 */
struct allocator_t
{
	allocator_alloc_func_t alloc;
	allocator_realloc_func_t realloc;
	allocator_free_func_t free;
};

void* allocator_alloc_wrapper(allocator_t* allocator, size_t count, size_t size, size_t align, const char* file, int line);
void* allocator_realloc_wrapper(allocator_t* allocator, void* memory, size_t count, size_t size, size_t align, const char* file, int line);
void allocator_free_wrapper(allocator_t* allocator, void* memory, const char* file, int line);

#define ALLOCATOR_ALLOC(allocator, size, align) allocator_alloc_wrapper(allocator, 1, size, align, __FILE__, __LINE__)
#define ALLOCATOR_ALLOC_TYPE(allocator, type) (type*)allocator_alloc_wrapper(allocator, 1, sizeof(type), ALIGNOF(type), __FILE__, __LINE__)
#define ALLOCATOR_ALLOC_TYPE_ALIGNED(allocator, type, align) (type*)allocator_alloc_wrapper(allocator, 1, sizeof(type), align, __FILE__, __LINE__)
#define ALLOCATOR_ALLOC_ARRAY(allocator, count, type) (type*)allocator_alloc_wrapper(allocator, count, sizeof(type), ALIGNOF(type), __FILE__, __LINE__)
#define ALLOCATOR_REALLOC(allocator, memory, size, align) allocator_realloc_wrapper(allocator, memory, 1, size, align, __FILE__, __LINE__)
#define ALLOCATOR_REALLOC_ARRAY(allocator, memory, count, type) (type*)allocator_realloc_wrapper(allocator, memory, count, sizeof(type), ALIGNOF(type), __FILE__, __LINE__)
#define ALLOCATOR_FREE(allocator, memory) allocator_free_wrapper(allocator, memory, __FILE__, __LINE__)

#if defined(COMPILER_MSVC)
#	define ALLOCATOR_NEW(allocator, type, ...) (new (allocator_alloc_wrapper(allocator, 1, sizeof(type), ALIGNOF(type), __FILE__, __LINE__)) type(__VA_ARGS__))
#elif defined(COMPILER_GCC)
#	define ALLOCATOR_NEW(allocator, type, args...) (new (allocator_alloc_wrapper(allocator, 1, sizeof(type), ALIGNOF(type), __FILE__, __LINE__)) type(##args))
#else
#	error not defined for this platform
#endif
#define ALLOCATOR_DELETE(allocator, type, ptr) do{ if(ptr){ (ptr)->~type(); allocator_free_wrapper(allocator, ptr, __FILE__, __LINE__); } }while(0)

extern allocator_t allocator_malloc;

allocator_t* allocator_incheap_create(allocator_t* parent, size_t size);
void allocator_incheap_destroy(allocator_t* allocator);
void allocator_incheap_reset(allocator_t* allocator, void* mark = nullptr);
void* allocator_incheap_start(allocator_t* allocator);
void* allocator_incheap_curr(allocator_t* allocator);
size_t allocator_incheap_bytes_consumed(allocator_t* allocator);

struct allocator_helper_t
{
	allocator_t* allocator;
	uintptr_t base_ptr;
	uintptr_t curr_ptr;
	uintptr_t base_align;
	uintptr_t size;
};

allocator_helper_t allocator_helper_create(allocator_t* allocator);
void* allocator_helper_commit(allocator_helper_t* helper);
void allocator_helper_destroy(allocator_helper_t* helper);
void allocator_helper_add(allocator_helper_t* helper, size_t size, size_t align);
void* allocator_helper_get(allocator_helper_t* helper, size_t size, size_t align);
size_t allocator_helper_size(allocator_helper_t* helper);

#define ALLOCATOR_HELPER_ADD(helper, n, type) allocator_helper_add(helper, n*sizeof(type), ALIGNOF(type))
#define ALLOCATOR_HELPER_GET(helper, n, type) (type*)allocator_helper_get(helper, n*sizeof(type), ALIGNOF(type))
