#include "render_common.h"

#include <foundation/assert.h>

#if defined(FAMILY_WINDOWS)
#	define RENDER_CALL_FUNC(render, func, backend, ...) \
	switch(backend) \
	{ \
		case RENDER_BACKEND_NULL: \
			return render_null_ ## func ((render_null_t*)render, __VA_ARGS__); \
		case RENDER_BACKEND_DX12: \
			return render_dx12_ ## func ((render_dx12_t*)render, __VA_ARGS__); \
		default: \
			BREAKPOINT(); \
	}
#elif defined(PLATFORM_OSX)
#	define RENDER_CALL_FUNC(render, func, backend, ...) \
	switch(backend) \
	{ \
		case RENDER_BACKEND_NULL: \
			return render_null_ ## func ((render_null_t*)render, ##__VA_ARGS__); \
		case RENDER_BACKEND_METAL: \
			return render_metal_ ## func ((render_metal_t*)render, ##__VA_ARGS__); \
		default: \
			BREAKPOINT(); \
	}
#else
#	error not implemented for the platform
#endif

render_result_t render_create(const render_create_info_t* create_info, render_t** out_render)
{
	render_result_t res = RENDER_RESULT_OK;
	render_t* render = nullptr;

	// TODO: backend selection
#if defined(FAMILY_WINDOWS)
	res = render_dx12_create(create_info, (render_dx12_t**)&render);
#elif defined(PLATFORM_OSX)
	res = render_metal_create(create_info, (render_metal_t**)&render);
#else
#	error not implemented for the platform
#endif

	*out_render = render;
	return res;
}

void render_destroy(render_t* render)
{
	RENDER_CALL_FUNC(render, destroy, render->backend)
}

void render_wait_idle(render_t* render)
{
	RENDER_CALL_FUNC(render, wait_idle, render->backend)
}

render_result_t render_view_create(render_t* render, const render_view_create_info_t* create_info, render_view_id_t* out_view_id)
{
	RENDER_CALL_FUNC(render, view_create, render->backend, create_info, out_view_id)
	return (render_result_t)-1;
}

void render_view_destroy(render_t* render, render_view_id_t view_id)
{
	RENDER_CALL_FUNC(render, view_destroy, render->backend, view_id)
}

render_result_t render_script_create(render_t* render, const render_script_create_info_t* create_info, render_script_id_t* out_script_id)
{
	RENDER_CALL_FUNC(render, script_create, render->backend, create_info, out_script_id)
	return (render_result_t)-1;
}

void render_script_destroy(render_t* render, render_script_id_t script_id)
{
	RENDER_CALL_FUNC(render, script_destroy, render->backend, script_id)
}

render_result_t render_texture_create(render_t* render, const texture_data_t* texture_data, render_texture_id_t* out_texture_id)
{
	RENDER_CALL_FUNC(render, texture_create, render->backend, texture_data, out_texture_id)
	return (render_result_t)-1;
}

void render_texture_destroy(render_t* render, render_texture_id_t texture_id)
{
	RENDER_CALL_FUNC(render, texture_destroy, render->backend, texture_id)
}

render_result_t render_shader_create(render_t* render, const shader_data_t* shader_data, render_shader_id_t* out_shader_id)
{
	RENDER_CALL_FUNC(render, shader_create, render->backend, shader_data, out_shader_id)
	return (render_result_t)-1;
}

void render_shader_destroy(render_t* render, render_shader_id_t shader_id)
{
	RENDER_CALL_FUNC(render, shader_destroy, render->backend, shader_id)
}

render_result_t render_material_create(render_t* render, const material_data_t* material_data, render_material_id_t* out_material_id)
{
	RENDER_CALL_FUNC(render, material_create, render->backend, material_data, out_material_id)
	return (render_result_t)-1;
}

void render_material_destroy(render_t* render, render_material_id_t material_id)
{
	RENDER_CALL_FUNC(render, material_destroy, render->backend, material_id)
}

render_result_t render_mesh_create(render_t* render, const mesh_data_t* mesh_data, render_mesh_id_t* out_mesh_id)
{
	RENDER_CALL_FUNC(render, mesh_create, render->backend, mesh_data, out_mesh_id)
	return (render_result_t)-1;
}

void render_mesh_destroy(render_t* render, render_mesh_id_t mesh_id)
{
	RENDER_CALL_FUNC(render, mesh_destroy, render->backend, mesh_id)
}

render_result_t render_instance_create(render_t* render, const render_instance_create_info_t* create_info, render_instance_id_t* out_instance_id)
{
	RENDER_CALL_FUNC(render, instance_create, render->backend, create_info, out_instance_id)
	return (render_result_t)-1;
}

void render_instance_destroy(render_t* render, render_instance_id_t instance_id)
{
	RENDER_CALL_FUNC(render, instance_destroy, render->backend, instance_id)
}

render_result_t render_instance_set_data(render_t* render, size_t num_instances, render_instance_id_t* instance_ids, render_instance_data_t* instance_data)
{
	RENDER_CALL_FUNC(render, instance_set_data, render->backend, num_instances, instance_ids, instance_data)
	return (render_result_t)-1;
}

void render_kick_render(render_t* render, render_view_id_t view_id, render_script_id_t script_id)
{
	RENDER_CALL_FUNC(render, kick_render, render->backend, view_id, script_id)
}

void render_kick_upload(render_t* render)
{
	RENDER_CALL_FUNC(render, kick_upload, render->backend)
}
