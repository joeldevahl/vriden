#include <foundation/assert.h>
#include <foundation/hash.h>
#include <foundation/allocator.h>
#include <foundation/resource_cache.h>

#include <graphics/render.h>

#include <dl/dl.h>

#include "resource_context.h"

#include <units/graphics/types/mesh.h>

static const unsigned char mesh_typelib[] =
{
#include <units/graphics/types/mesh.tlb.hex>
};

static bool mesh_create(void* context, allocator_t* allocator, void* creation_data, size_t size, void** out_resource_data, void** out_private_data)
{
	resource_context_t* resource_context = (resource_context_t*)context;
	render_t* render = resource_context->render;

	mesh_data_t* mesh_data = nullptr;
	size_t consumed = 0;
	dl_error_t err = dl_instance_load_inplace(
			resource_context->dl_ctx,
			mesh_data_t::TYPE_ID,
			(unsigned char*)creation_data,
			size,
			(void**)&mesh_data,
			&consumed);
	ASSERT(err == DL_ERROR_OK);

	render_mesh_id_t mesh_id;
	render_result_t res = render_mesh_create(render, mesh_data, &mesh_id);
	ASSERT(res == RENDER_RESULT_OK);

	*out_resource_data = (void*)(uintptr_t)mesh_id;
	return true;
}

static bool mesh_recreate(void* context, allocator_t* allocator, void* creation_data, size_t size, void* prev_resource_data, void* prev_private_data, void** out_resource_data, void** out_private_data)
{
	return false;
}

static void mesh_destroy(void* context, allocator_t* allocator, void* resource_data, void* private_data)
{
	resource_context_t* resource_context = (resource_context_t*)context;
	render_t* render = resource_context->render;
	render_mesh_id_t mesh_id = (render_mesh_id_t)(uintptr_t)resource_data;

	render_mesh_destroy(render, mesh_id);
}

void mesh_register_creator(resource_context_t* resource_context)
{
	dl_context_load_type_library(resource_context->dl_ctx, mesh_typelib, ARRAY_LENGTH(mesh_typelib));

	resource_creator_t mesh_creator = {
		mesh_create,
		mesh_recreate,
		mesh_destroy,
		&allocator_malloc,
		resource_context,
		hash_string("mesh")
	};
	resource_cache_add_creator(resource_context->resource_cache, &mesh_creator);
}
