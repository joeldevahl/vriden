#pragma once

struct window_create_params_t
{
	struct allocator_t* allocator;
	uint32_t width;
	uint32_t height;
	const char* name;
};

struct window_t;

window_t* window_create(window_create_params_t* params);
void window_destroy(window_t* window);

void* window_get_platform_handle(window_t* window);

uint32_t window_get_width(window_t* window);
uint32_t window_get_height(window_t* window);