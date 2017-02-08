#pragma once

#include <cstring> // for memcpy

#include <stdint.h>

#include "assert.h"
#include "allocator.h"

template<class T, class TH = uint16_t>
struct cobjpool_t
{
	allocator_t* _alloc;
	size_t       _capacity;
	size_t       _num_free;
	TH*          _handles;
	TH*          _indices;
	TH*          _allocated_handles;
	T*           _data;

	cobjpool_t() : _alloc(NULL), _capacity(0), _num_free(0), _handles(NULL), _indices(NULL), _allocated_handles(NULL), _data(NULL) { }

	cobjpool_t(allocator_t* alloc, size_t capacity) : _alloc(NULL), _capacity(0), _num_free(0), _handles(NULL), _indices(NULL), _allocated_handles(NULL), _data(NULL)
	{
		create(alloc, capacity);
	}

	~cobjpool_t()
	{
		ALLOCATOR_FREE(_alloc, _handles);
		ALLOCATOR_FREE(_alloc, _indices);
		ALLOCATOR_FREE(_alloc, _data);
		ALLOCATOR_FREE(_alloc, _allocated_handles);
	}

	void create(allocator_t* alloc, size_t capacity)
	{
		ASSERT(_handles == NULL && _indices == NULL && _allocated_handles == NULL && _data == NULL, "cobjpool was already created");

		_alloc = alloc;
		_capacity = capacity;
		_num_free = capacity;
		_handles = (TH*)ALLOCATOR_ALLOC(alloc, capacity*sizeof(TH), ALIGNOF(TH));
		_indices = (TH*)ALLOCATOR_ALLOC(alloc, capacity*sizeof(TH), ALIGNOF(TH));
		_allocated_handles = (TH*)ALLOCATOR_ALLOC(alloc, sizeof(TH)*capacity, ALIGNOF(TH));
		_data = (T*)ALLOCATOR_ALLOC(alloc, sizeof(T)*capacity, ALIGNOF(T));

		for(size_t i = 0; i < capacity; ++i)
			_handles[i] = static_cast<TH>(capacity - i - 1);
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
		ASSERT(_num_free > 0, "cobjpool out of space");

		TH offset = static_cast<TH>(_capacity - _num_free);
		TH handle = _handles[--_num_free];

		_indices[handle] = offset;
		_allocated_handles[offset] = handle;

		return handle;
	}

	void free_handle(TH handle)
	{
		size_t count = _capacity - _num_free;
		TH last = _allocated_handles[count - 1];

		if(handle != last)
		{
			TH ih = _indices[handle];
			TH il = _indices[last];
			T* dst = _data + ih;
			const T* src = _data + il;
			memcpy(dst, src, sizeof(T));
			_allocated_handles[ih] = _allocated_handles[il];
		}

		_indices[handle] = _indices[last];
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
		ASSERT(handle < _capacity, "bad handle");

		return _data + _indices[handle];
	}

	const T* handle_to_pointer(TH handle) const
	{
		ASSERT(handle < _capacity, "bad handle");

		return _data + _indices[handle];
	}

	TH pointer_to_handle(const T* ptr) const
	{
		ptrdiff_t index = ptr - _data;
		ASSERT(index < _capacity - _num_free, "bad pointer");
		return _allocated_handles[index];
	}

	T* base_ptr()
	{
		return _data;
	}

	const T* base_ptr() const
	{
		return _data;
	}

	TH* index_ptr()
	{
		return _indices;
	}

	const TH* index_ptr() const
	{
		return _indices;
	}

};
