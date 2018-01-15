#pragma once

#include "assert.h"
#include "allocator.h"

#include <cstring> // for memset

template<class TK, class T>
struct table_t
{
	struct val_t
	{
		val_t* next;
		TK key;
		T val;
	};

	struct bucket_t
	{
		val_t* start;
	};

	bucket_t* _buckets;
	val_t*  _vals;
	val_t* _free_list;

	size_t _capacity;
	size_t _num_buckets;

	void create(allocator_t* allocator, size_t capacity, size_t num_buckets)
	{
		ASSERT(_buckets == nullptr, "table was already created");
		ASSERT(_vals == nullptr, "table was already created");
		_capacity = capacity;
		_num_buckets = num_buckets;

		if(capacity)
		{
			_vals = (val_t*)ALLOCATOR_ALLOC(allocator, capacity * sizeof(val_t), ALIGNOF(val_t));
			_buckets = (bucket_t*)ALLOCATOR_ALLOC(allocator, num_buckets * sizeof(bucket_t), ALIGNOF(bucket_t));

			memset(_vals, 0, capacity * sizeof(val_t));
			memset(_buckets, 0, num_buckets * sizeof(bucket_t));

			for(size_t i = 0; i < capacity - 1; ++i)
			{
				_vals[i].next = &_vals[i+1];
			}
			_free_list = _vals;
		}
	}

	void destroy(allocator_t* allocator)
	{
		ALLOCATOR_FREE(allocator, _vals);
		ALLOCATOR_FREE(allocator, _buckets);
	}

	size_t capacity() const { return _capacity; }

	void insert(TK key, T val)
	{
		ASSERT(_free_list != nullptr, "capacity of table reached");
		size_t ib = (size_t)key % _num_buckets;
		bucket_t* b = &_buckets[ib];
		val_t* v = b->start;
		while(v)
		{
			if(v->key == key)
			{
				v->val = val;
				return;
			}
			v = v->next;
		}

		v = _free_list;
		_free_list = v->next;

		v->key = key;
		v->next = b->start;
		v->val = val;
	
		b->start = v;
	}

	T* fetch(TK key)
	{
		size_t ib = (size_t)key % _num_buckets;
		bucket_t* b = &_buckets[ib];
		val_t* v = b->start;
		while(v)
		{
			if(v->key == key)
				return &v->val;
			v = v->next;
		}
		return nullptr;
	}

	T& operator[](TK key)
	{
		T* val = fetch(key);
		ASSERT(val != nullptr);
		return *val;
	}

	const T& operator[](TK key) const
	{
		T* val = fetch(key);
		ASSERT(val != nullptr);
		return *val;
	}
	
	bool has_key(TK key)
	{
		size_t ib = (size_t)key % _num_buckets;
		bucket_t* b = &_buckets[ib];
		val_t* v = b->start;
		while(v)
		{
			if(v->key == key)
				return true;
			v = v->next;
		}
		return false;
	}

	void remove(TK key)
	{
		size_t ib = (size_t)key % _num_buckets;
		bucket_t* b = &_buckets[ib];
		val_t* v = b->start;
		val_t* vp = b->start;
		while(v)
		{
			if(v->key == key)
			{
				if(vp == v)
					b->start = v->next;
				else
					vp->next = v->next;
				v->next = _free_list;
				_free_list = v;
				return;
			}
			vp = v;
			v = v->next;
		}
	}
};
