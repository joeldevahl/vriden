#pragma once

#include "assert.h"
#include "allocator.h"
#include "defines.h"

#include <cstring> // for memmove

template<class T>
struct array_t
{
	T* _ptr;
	size_t _capacity;
	size_t _length;

	void create(allocator_t* allocator, size_t capacity)
	{
		_capacity = capacity;
		_length = 0;
		_ptr = capacity ? (T*)ALLOCATOR_ALLOC(allocator, capacity * sizeof(T), ALIGNOF(T)) : nullptr;
	}

	void create_with_length(allocator_t* allocator, size_t length)
	{
		create(allocator, length);
		set_length(length);
	}

	void destroy(allocator_t* allocator)
	{
		ALLOCATOR_FREE(allocator, _ptr);
	}

	void set_capacity(allocator_t* allocator, size_t new_capacity)
	{
		ASSERT(new_capacity >= _capacity, "array cannot shrink in size for now");

		_ptr = ALLOCATOR_REALLOC_ARRAY(allocator, _ptr, new_capacity, T);
		_capacity = new_capacity;
	}

	void grow(allocator_t* allocator, size_t amount = 0)
	{
		set_capacity(allocator, _capacity + (amount ? amount : _capacity));
	}

	void ensure_capacity(allocator_t* allocator, size_t new_capacity)
	{
		if (_capacity < new_capacity)
			set_capacity(allocator, new_capacity);
	}

	T& operator[](size_t i)
	{
		return _ptr[i];
	}

	const T& operator[](size_t i) const
	{
		return _ptr[i];
	}

	void set_length(size_t length)
	{
		ASSERT(length <= _capacity, "length greater than capacity");
		_length = length;
	}

	void clear()
	{
		set_length(0);
	}

	size_t capacity() const
	{
		return _capacity;
	}

	size_t length() const
	{
		return _length;
	}

	bool empty() const
	{
		return _length == 0;
	}

	bool any() const
	{
		return _length != 0;
	}

	bool full() const
	{
		return _length == _capacity;
	}

	T* begin()
	{
		return _ptr;
	}

	const T* begin() const
	{
		return _ptr;
	}

	T* end()
	{
		return _ptr + _length;
	}

	const T* end() const
	{
		return _ptr + _length;
	}

	T& front()
	{
		return _ptr[0];
	}

	const T& front() const
	{
		return _ptr[0];
	}

	T& back()
	{
		return _ptr[_length-1];
	}

	const T& back() const
	{
		return _ptr[_length-1];
	}

	void remove_front()
	{
		_length -= 1;
		memmove(_ptr, _ptr + 1, _length * sizeof(T));
	}

	void remove_front_fast()
	{
		_length -= 1;
		memmove(_ptr, _ptr + _length, sizeof(T));
	}

	void remove_back()
	{
		_length -= 1;
	}

	void remove_at(size_t i)
	{
		_length -= 1;
		memmove(_ptr + i, _ptr + i + 1, (_length - i) * sizeof(T));
	}

	void remove_at_fast(size_t i)
	{
		_length -= 1;
		memmove(_ptr + i, _ptr + _length, sizeof(T));
	}

	void append(const T& val)
	{
		ASSERT(_length < _capacity, "cannot add beyond capacity");
		size_t i = _length;
		memcpy(_ptr + i, &val, sizeof(T));
		_length += 1;
	}

	void insert_at(size_t i, const T& val)
	{
		ASSERT(_length < _capacity, "cannot add beyond capacity");
		ASSERT(i <= _length, "cannot insert outside current range");
		memmove(_ptr + i + 1, _ptr + i, (_length - i) * sizeof(T));
		memcpy(_ptr + i, &val, sizeof(T));
		_length += 1;
	}
};

template<class T>
struct scoped_array_t : public array_t<T>
{
	allocator_t* _allocator;

	scoped_array_t(allocator_t* allocator, size_t size = 0)
		: _allocator(allocator)
	{
		this->create(_allocator, size);
		this->set_length(size);
	}

	~scoped_array_t()
	{
		this->destroy(_allocator);
	}

	void grow(size_t amount = 0)
	{
		this->grow(_allocator, amount);
	}
/*
	void ensure_capacity(size_t new_capacity)
	{
		this->ensure_capacity(_allocator, new_capacity);
	}
*/	
	void grow_and_append(const T& val)
	{
		this->ensure_capacity(_allocator, length() + 1);
		this->append(val);
	}
};