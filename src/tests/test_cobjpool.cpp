#include "allocator.h"
#include "cobjpool.h"

#include "catch.hpp"

#define NUM_ELEMS 1024

TEST_CASE("CObj Pool can be created and destroyed", "[container]")
{
	cobjpool_t<uint16_t, uint16_t> pool(&allocator_malloc, NUM_ELEMS);
	TEST(pool.capacity() == NUM_ELEMS);

	uint16_t ids[NUM_ELEMS];
	for(uint16_t i = 0; i < NUM_ELEMS; ++i)
	{
		ids[i] =  pool.alloc_handle();
	}

	for(uint16_t i = 0; i < NUM_ELEMS; ++i)
	{
		pool.free_handle(ids[i]);
	}
}
