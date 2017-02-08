#include <foundation/assert.h>
#include <foundation/allocator.h>
#include <foundation/defines.h>

#include <cstdlib>

void* allocator_alloc_wrapper(allocator_t* allocator, size_t count, size_t size, size_t align, const char* file, int line)
{
	return allocator->alloc(allocator, count, size, align, file, line);
}

void* allocator_realloc_wrapper(allocator_t* allocator, void* memory, size_t count, size_t size, size_t align, const char* file, int line)
{
	return allocator->realloc(allocator, memory, count, size, align, file, line);
}

void allocator_free_wrapper(allocator_t* allocator, void* memory, const char* file, int line)
{
	allocator->free(allocator, memory, file, line);
}

void* allocator_malloc_alloc(allocator_t* allocator, size_t count, size_t size, size_t align, const char* file, int line)
{
	(void)allocator;
	(void)file;
	(void)line;
#if defined(FAMILY_WINDOWS)
	return _aligned_malloc(count * size, align);
#elif defined(FAMILY_UNIX)
	void* ptr = nullptr;
	posix_memalign(&ptr, align<sizeof(void*)?sizeof(void*):align, count * size);
	return ptr;
#else
#	error Not implemented for this platform.
#endif
}

void* allocator_malloc_realloc(allocator_t* allocator, void* memory, size_t count, size_t size, size_t align, const char* file, int line)
{
	(void)allocator;
	(void)file;
	(void)line;
#if defined(FAMILY_WINDOWS)
	return _aligned_recalloc(memory, count, size, align);
#elif defined(FAMILY_UNIX)
	return realloc(memory, count * size);
#else
#	error Not implemented for this platform.
#endif
}

void allocator_malloc_free(allocator_t* allocator, void* memory, const char* file, int line)
{
	(void)allocator;
	(void)file;
	(void)line;
#if defined(FAMILY_WINDOWS)
	_aligned_free(memory);
#elif defined(FAMILY_UNIX)
	free(memory);
#else
#	error Not implemented for this platform.
#endif
}

allocator_t allocator_malloc = {
	allocator_malloc_alloc,
	allocator_malloc_realloc,
	allocator_malloc_free
};

struct allocator_incheap_t : public allocator_t
{
	allocator_t* parent;

	uint8_t* baseptr;
	size_t offset;
	size_t size;
};

void* allocator_incheap_alloc(allocator_t* allocator, size_t count, size_t size, size_t align, const char* file, int line)
{
	(void)file;
	(void)line;
	allocator_incheap_t* incheap = (allocator_incheap_t*)allocator;
	uint8_t* res = (uint8_t*)ALIGN_UP(incheap->baseptr + incheap->offset, align);
	incheap->offset = res - incheap->baseptr + count*size;
	ASSERT(incheap->offset <= incheap->size, "Incheap out of space.");
	return res;
}

void* allocator_incheap_realloc(allocator_t* allocator, void* memory, size_t count, size_t size, size_t align, const char* file, int line)
{
	(void)allocator;
	(void)memory;
	(void)count;
	(void)size;
	(void)align;
	(void)file;
	(void)line;
	ASSERT(0, "Cannot realloc on incheap");
	return 0x0;
}

void allocator_incheap_free(allocator_t* allocator, void* memory, const char* file, int line)
{
	(void)allocator;
	(void)memory;
	(void)file;
	(void)line;
	// NOP.
}

void allocator_incheap_reset(allocator_t* allocator, void* mark)
{
	allocator_incheap_t* incheap = (allocator_incheap_t*)allocator;
	uintptr_t new_offset = mark ? reinterpret_cast<uint8_t*>(mark) - incheap->baseptr : 0;
	ASSERT(new_offset >= 0);
	ASSERT(new_offset < incheap->size);
	ASSERT(new_offset <= incheap->offset);
	incheap->offset = new_offset;
}

void* allocator_incheap_start(allocator_t* allocator)
{
	allocator_incheap_t* incheap = (allocator_incheap_t*)allocator;
	return incheap->baseptr;
}

void* allocator_incheap_curr(allocator_t* allocator)
{
	allocator_incheap_t* incheap = (allocator_incheap_t*)allocator;
	return incheap->baseptr + incheap->offset;
}

size_t allocator_incheap_bytes_consumed(allocator_t* allocator)
{
	allocator_incheap_t* incheap = (allocator_incheap_t*)allocator;
	return incheap->offset;
}

allocator_t* allocator_incheap_create(allocator_t* parent, size_t num_bytes)
{
	allocator_incheap_t* incheap = 0x0;
	uint8_t* ptr = (uint8_t*)ALLOCATOR_ALLOC(parent, num_bytes + sizeof(allocator_incheap_t), 16);
	ASSERT(ptr, "Could not allocate incheap data");

	incheap = (allocator_incheap_t*)ptr;
	incheap->alloc = allocator_incheap_alloc;
	incheap->realloc = allocator_incheap_realloc;
	incheap->free = allocator_incheap_free;
	incheap->parent = parent;
	incheap->baseptr = ptr + sizeof(allocator_incheap_t);
	incheap->offset = 0;
	incheap->size = num_bytes;

	return (allocator_t*)incheap;
}

void allocator_incheap_destroy(allocator_t* allocator)
{
	allocator_incheap_t* incheap = (allocator_incheap_t*)allocator;
	ALLOCATOR_FREE(incheap->parent, incheap);
}

allocator_helper_t allocator_helper_create(allocator_t* allocator)
{
	allocator_helper_t res = {
		allocator,
		0,
		0,
		0,
	};

	return res;
}

void* allocator_helper_commit(allocator_helper_t* helper)
{
	ASSERT(helper->base_ptr == 0, "Cannot commit twice");
	ASSERT(helper->allocator != NULL, "No allocator");
	void* res = ALLOCATOR_ALLOC(helper->allocator, helper->size, helper->base_align);
	helper->base_ptr = helper->curr_ptr = (uintptr_t)res;
	return res;
}

void allocator_helper_destroy(allocator_helper_t* helper)
{
	ASSERT(helper->allocator != NULL, "No allocator");
	ALLOCATOR_FREE(helper->allocator, (void*)helper->base_ptr);
}

void allocator_helper_add(allocator_helper_t* helper, uintptr_t size, uintptr_t align)
{
	// base align if this is the first add
	if(helper->base_align == 0)
		helper->base_align = align;
	else
		size += align-1; // reserve room for worst case misaligned start address
	helper->size += size;
}

void* allocator_helper_get(allocator_helper_t* helper, uintptr_t size, uintptr_t align)
{
	uintptr_t ptr = ALIGN_UP(helper->curr_ptr, align);
	void* res = (void*)ptr;
	ASSERT(ptr + size - helper->base_ptr <= helper->size, "Buffer overrun");
	helper->curr_ptr = ptr + size;
	return res;
}

size_t allocator_helper_size(allocator_helper_t* helper)
{
	return helper->size;
}
