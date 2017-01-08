#include "allocator.h"
#include "idpool.h"

#include "catch.hpp"

TEST_CASE("ID Pools can be created and destroyed", "[container]")
{
	idpool_t<uint16_t> pool(&allocator_default, 8);
	TEST(pool.capacity() == 8);

	for(uint16_t i = 0; i < 8; ++i)
	{
		TEST(pool.alloc_handle() == i);
	}

	for(uint16_t i = 0; i < 8; ++i)
	{
		pool.free_handle(i);
	}
}
