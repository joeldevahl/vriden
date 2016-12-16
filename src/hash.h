#pragma once

#include <stdint.h>
#include <cstdlib>

uint32_t hash_string(const char* str);
uint32_t hash_buffer(const void* buf, size_t size);
