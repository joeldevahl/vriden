#include <foundation/assert.h>
#include <foundation/hash.h>
#include <foundation/allocator.h>
#include <foundation/resource_cache.h>

#include <graphics/render.h>

#include <dl/dl.h>

#include "resource_context.h"

#include <units/graphics/types/shader.h>

static const unsigned char shader_typelib[] =
{
#include <units/graphics/types/shader.tlb.hex>
};

static bool shader_create(void* context, allocator_t* allocator, void* creation_data, size_t size, void** out_resource_data, void** out_private_data)
{
	resource_context_t* resource_context = (resource_context_t*)context;
	render_t* render = resource_context->render;

	shader_data_t* shader_data = nullptr;
	size_t consumed = 0;
	dl_error_t err = dl_instance_load_inplace(
			resource_context->dl_ctx,
			shader_data_t::TYPE_ID,
			(unsigned char*)creation_data,
			size,
			(void**)&shader_data,
			&consumed);
	ASSERT(err == DL_ERROR_OK);

	render_shader_id_t shader_id;
	render_result_t res = render_shader_create(render, shader_data, &shader_id);
	ASSERT(res == RENDER_RESULT_OK);

	*out_resource_data = (void*)(uintptr_t)shader_id;
	return true;
}

static bool shader_recreate(void* context, allocator_t* allocator, void* creation_data, size_t size, void* prev_resource_data, void* prev_private_data, void** out_resource_data, void** out_private_data)
{
	return false;
}

static void shader_destroy(void* context, allocator_t* allocator, void* resource_data, void* private_data)
{
	resource_context_t* resource_context = (resource_context_t*)context;
	render_t* render = resource_context->render;
	render_shader_id_t shader_id = (render_shader_id_t)(uintptr_t)resource_data;

	render_shader_destroy(render, shader_id);
}

void shader_register_creator(resource_context_t* resource_context)
{
	dl_context_load_type_library(resource_context->dl_ctx, shader_typelib, ARRAY_LENGTH(shader_typelib));

	resource_creator_t shader_creator = {
		shader_create,
		shader_recreate,
		shader_destroy,
		&allocator_malloc,
		resource_context,
		hash_string("shader")
	};
	resource_cache_add_creator(resource_context->resource_cache, &shader_creator);
}
