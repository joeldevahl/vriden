#include "render_metal.h"
#include <foundation/allocator.h>
#include <foundation/hash.h>

#include <algorithm>

render_result_t render_metal_create(const render_create_info_t* create_info, render_metal_t** out_render)
{
	render_metal_t* render = ALLOCATOR_NEW(create_info->allocator, render_metal_t);
	render->allocator = create_info->allocator;
	render->backend = RENDER_BACKEND_METAL;

	*out_render = render;
	return RENDER_RESULT_OK;
}

void render_metal_destroy(render_metal_t* render)
{
	ALLOCATOR_DELETE(render->allocator, render_metal_t, render);
}

void render_metal_wait_idle(render_metal_t* /*render*/)
{
}

render_result_t render_metal_view_create(render_metal_t* /*render*/, const render_view_create_info_t* /*create_info*/, render_view_id_t* /*out_view_id*/)
{
	return RENDER_RESULT_OK;
}

void render_metal_view_destroy(render_metal_t* /*render*/, render_view_id_t /*view_id*/)
{
}

render_result_t render_metal_script_create(render_metal_t* /*render*/, const render_script_create_info_t* /*create_info*/, render_script_id_t* /*out_script_id*/)
{
	return RENDER_RESULT_OK;
}

void render_metal_script_destroy(render_metal_t* /*render*/, render_script_id_t /*script_id*/)
{
}

render_result_t render_metal_texture_create(render_metal_t* /*render*/, const texture_data_t* /*texture_data*/, render_texture_id_t* /*out_texture_id*/)
{
	return RENDER_RESULT_OK;
}

void render_metal_texture_destroy(render_metal_t* /*render*/, render_texture_id_t /*texture_id*/)
{
}

render_result_t render_metal_shader_create(render_metal_t* /*render*/, const shader_data_t* /*shader_data*/, render_shader_id_t* /*out_shader_id*/)
{
	return RENDER_RESULT_OK;
}

void render_metal_shader_destroy(render_metal_t* /*render*/, render_shader_id_t /*shader_id*/)
{
}

render_result_t render_metal_material_create(render_metal_t* /*render*/, const material_data_t* /*material_data*/, render_material_id_t* /*out_material_id*/)
{
	return RENDER_RESULT_OK;
}

void render_metal_material_destroy(render_metal_t* /*render*/, render_material_id_t /*material_id*/)
{
}

render_result_t render_metal_mesh_create(render_metal_t* /*render*/, const mesh_data_t* /*mesh_data*/, render_mesh_id_t* /*out_mesh_id*/)
{
	return RENDER_RESULT_OK;
}

void render_metal_mesh_destroy(render_metal_t* /*render*/, render_mesh_id_t /*mesh_id*/)
{
}

render_result_t render_metal_instance_create(render_metal_t* /*render*/, const render_instance_create_info_t* /*create_info*/, render_instance_id_t* /*out_instance_id*/)
{
	return RENDER_RESULT_OK;
}

void render_metal_instance_destroy(render_metal_t* /*render*/, render_instance_id_t /*instance_id*/)
{
}

render_result_t render_metal_instance_set_data(render_metal_t* /*render*/, size_t /*num_instances*/, render_instance_id_t* /*instance_ids*/, render_instance_data_t* /*instance_data*/)
{
	return RENDER_RESULT_OK;
}

void render_metal_kick_render(render_metal_t* /*render*/, render_view_id_t /*view_id*/, render_script_id_t /*script_id*/)
{
}

void render_metal_kick_upload(render_metal_t* /*render*/)
{
}
