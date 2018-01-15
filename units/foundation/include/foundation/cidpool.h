#pragma once

#include "assert.h"
#include "allocator.h"

template<class TH = uint16_t>
struct cidpool_t
{
	size_t       _capacity;
	size_t       _num_free;
	TH*          _handles;
	TH*          _indices;

	void create(allocator_t* allocator, size_t capacity)
	{
		ASSERT(_handles == NULL && _indices == NULL, "cidpool was already created");
		_capacity = capacity;
		_num_free = capacity;
		_handles = (TH*)ALLOCATOR_ALLOC(allocator, capacity*sizeof(TH), alignof(TH));
		_indices = (TH*)ALLOCATOR_ALLOC(allocator, capacity*sizeof(TH), alignof(TH));

		for(size_t i = 0; i < capacity; ++i)
			_handles[i] = static_cast<TH>(capacity - i - 1);
	}
	
	void destroy(allocator_t* allocator)
	{
		ALLOCATOR_FREE(allocator, _handles);
		ALLOCATOR_FREE(allocator, _indices);
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
		ASSERT(_num_free > 0, "cidpool out of space");

		TH index = static_cast<TH>(_num_free - 1);
		TH handle = _handles[--_num_free];

		_indices[handle] = index;

		return handle;
	}

	void free_handle_and_move(TH handle_to_remove, TH handle_to_move)
	{
		ASSERT(_num_free < _capacity, "tried to free handle on empty cidpool");
		ASSERT(handle_to_remove < _capacity, "bad handle");
		ASSERT(handle_to_move < _capacity, "bad handle");

		_indices[handle_to_move] = _indices[handle_to_remove];
		_handles[_num_free++] = handle_to_remove;
	}

	TH handle_to_index(TH handle)
	{
		ASSERT(handle < _capacity, "bad handle");

		return _indices[handle];
	}
};
