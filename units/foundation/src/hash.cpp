#include <string.h>

#include <foundation/hash.h>

uint32_t hash_string(const char* str)
{
	return hash_buffer(str, strlen(str));
}

uint32_t hash_buffer(const void* buf, size_t size)
{
	// TODO: replace with some sane implementation
	const char* key = (const char*)buf;
	uint32_t hash, i;
	for (hash = i = 0; i < size; ++i)
	{
		hash += key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	return hash;
}
