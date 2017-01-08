#include "allocator.h"
#include "table.h"

#include "catch.hpp"

TEST_CASE("Tables can be created and destroyed", "[container]")
{
	table_t<uint16_t, uint16_t> table(&allocator_malloc, 8, 2);
	REQUIRE(table.capacity() == 8);

	table.insert(1, 32);
	uint16_t* tmp = table.fetch(1);
	REQUIRE(tmp != NULL);
	REQUIRE(*tmp == 32);

	uint16_t val = 33;
	table[1] = val;
	val = table[1];
	REQUIRE(val == 33);
}
