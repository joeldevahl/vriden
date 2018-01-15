#pragma once

#include "assert.h"
#include "allocator.h"

template<class TH = uint16_t>
struct idpool_t
{
	size_t       _capacity;
	size_t       _num_free;
	TH*          _handles;

	void create(allocator_t* allocator, size_t capacity)
	{
		ASSERT(_handles == NULL, "idpool was already created");

		_capacity = capacity;
		_num_free = capacity;
		_handles = ALLOCATOR_ALLOC_ARRAY(allocator, capacity, TH);

		for(size_t i = 0; i < capacity; ++i)
			_handles[i] = static_cast<TH>(capacity - i - 1);
	}

	void destroy(allocator_t* allocator)
	{
		ALLOCATOR_FREE(allocator, _handles);
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
		ASSERT(_num_free > 0, "idpool out of space");

		return _handles[--_num_free];
	}

	void free_handle(TH handle)
	{
		ASSERT(_num_free < _capacity, "tried to free handle on empty idpool");

		_handles[_num_free++] = handle;
	}
};
