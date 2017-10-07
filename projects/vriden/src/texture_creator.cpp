#include <foundation/assert.h>
#include <foundation/hash.h>
#include <foundation/allocator.h>
#include <foundation/resource_cache.h>

#include <graphics/render.h>

#include <dl/dl.h>

#include "resource_context.h"

#include <units/graphics/types/texture.h>

static const unsigned char texture_typelib[] =
{
#include <units/graphics/types/texture.tlb.hex>
};

static bool texture_create(void* context, allocator_t* allocator, void* creation_data, size_t size, void** out_resource_data, void** out_private_data)
{
	resource_context_t* resource_context = (resource_context_t*)context;
	render_t* render = resource_context->render;

	texture_data_t* texture_data = nullptr;
	size_t consumed = 0;
	dl_error_t err = dl_instance_load_inplace(
			resource_context->dl_ctx,
			texture_data_t::TYPE_ID,
			(unsigned char*)creation_data,
			size,
			(void**)&texture_data,
			&consumed);
	ASSERT(err == DL_ERROR_OK);

	render_texture_id_t texture_id;
	render_result_t res = render_texture_create(render, texture_data, &texture_id);
	ASSERT(res == RENDER_RESULT_OK);

	*out_resource_data = (void*)(uintptr_t)texture_id;
	return true;
}

static bool texture_recreate(void* context, allocator_t* allocator, void* creation_data, size_t size, void* prev_resource_data, void* prev_private_data, void** out_resource_data, void** out_private_data)
{
	return false;
}

static void texture_destroy(void* context, allocator_t* allocator, void* resource_data, void* private_data)
{
	resource_context_t* resource_context = (resource_context_t*)context;
	render_t* render = resource_context->render;
	render_texture_id_t texture_id = (render_texture_id_t)(uintptr_t)resource_data;

	render_texture_destroy(render, texture_id);
}

void texture_register_creator(resource_context_t* resource_context)
{
	dl_context_load_type_library(resource_context->dl_ctx, texture_typelib, ARRAY_LENGTH(texture_typelib));

	resource_creator_t texture_creator = {
		texture_create,
		texture_recreate,
		texture_destroy,
		&allocator_malloc,
		resource_context,
		hash_string("texture")
	};
	resource_cache_add_creator(resource_context->resource_cache, &texture_creator);
}
