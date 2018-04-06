#pragma once

#include <graphics/render.h>
#include "render_common.h"

struct render_metal_t : public render_t
{
	allocator_t* allocator;
};
