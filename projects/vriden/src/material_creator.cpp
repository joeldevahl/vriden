#include <foundation/assert.h>
#include <foundation/hash.h>
#include <foundation/allocator.h>
#include <foundation/resource_cache.h>

#include <graphics/render.h>

#include <dl/dl.h>

#include "resource_context.h"

#include <units/graphics/types/material.h>

static const unsigned char material_typelib[] =
{
#include <units/graphics/types/material.tlb.hex>
};

struct material_private_data_t
{
	resource_handle_t shader_handle;
};

static bool material_create(void* context, allocator_t* allocator, void* creation_data, size_t size, void** out_resource_data, void** out_private_data)
{
	resource_context_t* resource_context = (resource_context_t*)context;
	render_t* render = resource_context->render;

	material_data_t* material_data = nullptr;
	size_t consumed = 0;
	dl_error_t err = dl_instance_load_inplace(
			resource_context->dl_ctx,
			material_data_t::TYPE_ID,
			(unsigned char*)creation_data,
			size,
			(void**)&material_data,
			&consumed);
	ASSERT(err == DL_ERROR_OK);

	for (size_t ip = 0; ip < material_data->properties.count; ++ip)
	{
		// resolve all property resources
	}


	material_private_data_t* private_data = ALLOCATOR_ALLOC_TYPE(allocator, material_private_data_t);
	resource_cache_result_t resource_res = resource_cache_get_by_hash(resource_context->resource_cache, material_data->shader_name_hash, &private_data->shader_handle);
	ASSERT(resource_res == RESOURCE_CACHE_RESULT_OK);
	void* shader_ptr;
	resource_cache_handle_to_pointer(resource_context->resource_cache, private_data->shader_handle, &shader_ptr);
	render_shader_id_t shader_id = (render_shader_id_t)(uintptr_t)shader_ptr;

	// TODO: ugly hack to switch hash to ID. have to be done inside render later
	material_data->shader_name_hash = shader_id;

	render_material_id_t material_id;
	render_result_t res = render_material_create(render, material_data, &material_id);
	ASSERT(res == RENDER_RESULT_OK);

	*out_resource_data = (void*)(uintptr_t)material_id;
	*out_private_data = (void*)(uintptr_t)private_data;

	return true;
}

static bool material_recreate(void* context, allocator_t* allocator, void* creation_data, size_t size, void* prev_resource_data, void* prev_private_data, void** out_resource_data, void** out_private_data)
{
	return false;
}

static void material_destroy(void* context, allocator_t* allocator, void* resource_data, void* private_data)
{
	resource_context_t* resource_context = (resource_context_t*)context;
	render_t* render = resource_context->render;
	render_material_id_t material_id = (render_material_id_t)(uintptr_t)resource_data;

	render_material_destroy(render, material_id);

	resource_cache_release_handle(resource_context->resource_cache, reinterpret_cast<material_private_data_t*>(private_data)->shader_handle);
	ALLOCATOR_FREE(allocator, private_data);
}

void material_register_creator(resource_context_t* resource_context)
{
	dl_context_load_type_library(resource_context->dl_ctx, material_typelib, ARRAY_LENGTH(material_typelib));

	resource_creator_t material_creator = {
		material_create,
		material_recreate,
		material_destroy,
		&allocator_malloc,
		resource_context,
		hash_string("material")
	};
	resource_cache_add_creator(resource_context->resource_cache, &material_creator);
}
