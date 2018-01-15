#pragma once

#include "assert.h"
#include "allocator.h"

template<class T>
struct circular_queue_t
{
	T* _ptr;
	size_t _capacity;
	size_t _count;
	size_t _put;

	void create(allocator_t* allocator, size_t capacity)
	{
		ASSERT(_ptr == NULL, "queue was already created");
		_capacity = capacity;
		_count = 0;
		_put = 0;
		_ptr = (T*)ALLOCATOR_ALLOC(allocator, capacity * sizeof(T), ALIGNOF(T));
	}
	
	void destroy(allocator_t* allocator)
	{
		ALLOCATOR_FREE(allocator, _ptr);
	}

	bool empty() const
	{
		return _count == 0;
	}

	bool any() const
	{
		return _count != 0;
	}

	bool full() const
	{
		return _count == _capacity;
	}

	size_t space_left() const
	{
		return _capacity - _count;
	}

	void put(const T& t)
	{
		ASSERT(!full(), "cannot put into full queue");

		_ptr[_put] = t;
		_put = (_put + 1) % _capacity;
		_count++;
	}

	T get()
	{
		ASSERT(any(), "cannot get from empty queue");

		size_t i = (_put - _count) % _capacity;
		_count--;
		return _ptr[i];
	}
};
