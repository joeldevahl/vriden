#pragma once

#include "assert.h"
#include "allocator.h"

template<class T, class TH = uint16_t>
struct objpool_t
{
	size_t       _capacity;
	size_t       _num_free;
	TH*          _handles;
	T*           _data;

	void create(allocator_t* allocator, size_t capacity)
	{
		ASSERT(_handles == NULL, "objpool was already created");
		ASSERT(_data == NULL, "objpool alrady created");

		_capacity = capacity;
		_num_free = capacity;
		_handles = (TH*)ALLOCATOR_ALLOC(allocator, capacity*sizeof(TH), ALIGNOF(TH));
		_data = (T*)ALLOCATOR_ALLOC(allocator, sizeof(T)*capacity, ALIGNOF(T));

		for(size_t i = 0; i < capacity; ++i)
			_handles[i] = static_cast<TH>(capacity - i - 1);
	}

	void destroy(allocator_t* allocator)
	{
		ALLOCATOR_FREE(allocator, _handles);
		ALLOCATOR_FREE(allocator, _data);
	}

	bool full() const
	{
		return _num_free == 0;
	}
	
	bool empty() const
	{
		return _num_free == _capacity;
	}

	size_t capacity() const
	{
		return _capacity;
	}

	size_t num_free() const
	{
		return _num_free;
	}

	size_t num_used() const
	{
		return _capacity - _num_free;
	}

	TH alloc_handle()
	{
		ASSERT(_num_free > 0, "objpool out of space");

		return _handles[--_num_free];
	}

	void free_handle(TH handle)
	{
		ASSERT(_num_free < _capacity, "tried to free handle on empty objpool");

		_handles[_num_free++] = handle;
	}

	T* alloc()
	{
		return handle_to_pointer(alloc_handle());
	}

	void free(T* ptr)
	{
		free_handle(pointer_to_handle(ptr));
	}

	T* handle_to_pointer(TH handle)
	{
		return _data + handle;
	}

	TH pointer_to_handle(const T* ptr) const
	{
		TH handle = static_cast<TH>(ptr - _data);
		ASSERT(handle < capacity(), "pointer not in objpool");
		return handle;
	}
};
