#pragma once

#include "array.h"

template<class T = uint32_t>
struct range_pool_t
{
	struct range_t
	{
		T begin;
		T end;
	};

	array_t<range_t> _ranges;
	T _num_ids;

	void create(allocator_t* alloc, T num_ids, size_t range_capacity = 0)
	{
		_ranges.create(alloc, range_capacity ? range_capacity : (num_ids / 4));
		_num_ids = num_ids;

		range_t range = {
			0,
			num_ids,
		};

		_ranges.append(range);
	}
	
	void destroy(allocator_t* allocator)
	{
		_ranges.destroy(allocator);
	}

	T alloc(const T count)
	{
		for(size_t i = 0; i < _ranges.length(); ++i)
		{
			range_t& range = _ranges[i];
			T range_count = range.end - range.begin;
			if(range_count == count)
			{
				T offset = range.begin;
				_ranges.remove_at(i);
				return offset;
			}
			else if(range_count > count)
			{
				T offset = range.begin;
				range.begin += count;
				return offset;
			}
		}

		BREAKPOINT();
		return (T)-1;
	}

	void free(const T offset, const T count)
	{
		const size_t len = _ranges.length();
		const T begin = offset;
		const T end = offset + count;
		for(size_t i = 0; i < len; ++i)
		{
			range_t& range = _ranges[i];
			if(end < range.begin)
			{
				// The whole range is below the current range
				// Insert a new slot in the range list
				range_t new_range = { begin, end };
				_ranges.insert_at(i, new_range);
			}
			else if(end == range.begin)
			{
				// The returned range ends where the current range starts
				range.begin = begin;
				return;
			}
			else if(range.end == begin)
			{
				// The returned range starts where the current range ends
				if(i+1 < len && end == _ranges[i+1].begin)
				{
					// We have filled a gap, merge the whole range (curr, returned, next)
					_ranges[i+1].begin = range.begin;
					_ranges.remove_at(i);
				}
				else
				{
					// No gap filled, append returned range to the current
					range.end = end;
				}
				return;
			}
		}
	}
};
