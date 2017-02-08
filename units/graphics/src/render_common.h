#pragma once

#include <graphics/render.h>

struct render_t
{
	render_backend_t backend;
};

struct render_null_t;
struct render_dx12_t;

render_result_t render_null_create            (const render_create_info_t* create_info, render_null_t** out_render);
void            render_null_destroy           (render_null_t* render);
void            render_null_wait_idle         (render_null_t* render);
render_result_t render_null_view_create       (render_null_t* render, const render_view_create_info_t* create_info, render_view_id_t* out_view_id);
void            render_null_view_destroy      (render_null_t* render, render_view_id_t view_id);
render_result_t render_null_script_create     (render_null_t* render, const render_script_create_info_t* create_info, render_script_id_t* out_script_ud);
void            render_null_script_destroy    (render_null_t* render, render_script_id_t script_id);
render_result_t render_null_texture_create    (render_null_t* render, const render_texture_create_info_t* create_info, render_texture_id_t* out_texture_id);
void            render_null_texture_destroy   (render_null_t* render, render_texture_id_t texture_id);
render_result_t render_null_shader_create     (render_null_t* render, const shader_data_t* shader_data, render_shader_id_t* out_shader_id);
void            render_null_shader_destroy    (render_null_t* render, render_shader_id_t shader_id);
render_result_t render_null_material_create   (render_null_t* render, const material_data_t* material_data, render_material_id_t* out_material_id);
void            render_null_material_destroy  (render_null_t* render, render_material_id_t material_id);
render_result_t render_null_mesh_create       (render_null_t* render, const mesh_data_t* mesh_data, render_mesh_id_t* out_mesh_id);
void            render_null_mesh_destroy      (render_null_t* render, render_mesh_id_t mesh_id);
render_result_t render_null_instance_create   (render_null_t* render, const render_instance_create_info_t* create_info, render_instance_id_t* out_instance_id);
void            render_null_instance_destroy  (render_null_t* render, render_instance_id_t instance_id);
render_result_t render_null_instance_set_data (render_null_t* render, size_t num_instances, render_instance_id_t* instance_ids, render_instance_data_t* instance_data);
void            render_null_kick_render       (render_null_t* render, render_view_id_t view_id, render_script_id_t script_id); // TODO: multiple views (stereo, shadow, etc)
void            render_null_kick_upload       (render_null_t* render);

render_result_t render_dx12_create            (const render_create_info_t* create_info, render_dx12_t** out_render);
void            render_dx12_destroy           (render_dx12_t* render);
void            render_dx12_wait_idle         (render_dx12_t* render);
render_result_t render_dx12_view_create       (render_dx12_t* render, const render_view_create_info_t* create_info, render_view_id_t* out_view_id);
void            render_dx12_view_destroy      (render_dx12_t* render, render_view_id_t view_id);
render_result_t render_dx12_script_create     (render_dx12_t* render, const render_script_create_info_t* create_info, render_script_id_t* out_script_ud);
void            render_dx12_script_destroy    (render_dx12_t* render, render_script_id_t script_id);
render_result_t render_dx12_texture_create    (render_dx12_t* render, const render_texture_create_info_t* create_info, render_texture_id_t* out_texture_id);
void            render_dx12_texture_destroy   (render_dx12_t* render, render_texture_id_t texture_id);
render_result_t render_dx12_shader_create     (render_dx12_t* render, const shader_data_t* shader_data, render_shader_id_t* out_shader_id);
void            render_dx12_shader_destroy    (render_dx12_t* render, render_shader_id_t shader_id);
render_result_t render_dx12_material_create   (render_dx12_t* render, const material_data_t* material_data, render_material_id_t* out_material_id);
void            render_dx12_material_destroy  (render_dx12_t* render, render_material_id_t material_id);
render_result_t render_dx12_mesh_create       (render_dx12_t* render, const mesh_data_t* mesh_data, render_mesh_id_t* out_mesh_id);
void            render_dx12_mesh_destroy      (render_dx12_t* render, render_mesh_id_t mesh_id);
render_result_t render_dx12_instance_create   (render_dx12_t* render, const render_instance_create_info_t* create_info, render_instance_id_t* out_instance_id);
void            render_dx12_instance_destroy  (render_dx12_t* render, render_instance_id_t instance_id);
render_result_t render_dx12_instance_set_data (render_dx12_t* render, size_t num_instances, render_instance_id_t* instance_ids, render_instance_data_t* instance_data);
void            render_dx12_kick_render       (render_dx12_t* render, render_view_id_t view_id, render_script_id_t script_id); // TODO: multiple views (stereo, shadow, etc)
void            render_dx12_kick_upload       (render_dx12_t* render);
