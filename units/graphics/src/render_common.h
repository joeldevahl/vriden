#pragma once

#include <graphics/render.h>

struct render_t
{
	render_backend_t backend;
};

struct render_null_t;

render_result_t render_null_create            (const render_create_info_t* create_info, render_null_t** out_render);
void            render_null_destroy           (render_null_t* render);
void            render_null_wait_idle         (render_null_t* render);
render_result_t render_null_view_create       (render_null_t* render, const render_view_create_info_t* create_info, render_view_id_t* out_view_id);
void            render_null_view_destroy      (render_null_t* render, render_view_id_t view_id);
render_result_t render_null_script_create     (render_null_t* render, const render_script_create_info_t* create_info, render_script_id_t* out_script_ud);
void            render_null_script_destroy    (render_null_t* render, render_script_id_t script_id);
render_result_t render_null_texture_create    (render_null_t* render, const texture_data_t* texture_data, render_texture_id_t* out_texture_id);
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

#if defined(FAMILY_WINDOWS) || defined(PLATFORM_LINUX)
struct render_vulkan_t;

render_result_t render_vulkan_create            (const render_create_info_t* create_info, render_vulkan_t** out_render);
void            render_vulkan_destroy           (render_vulkan_t* render);
void            render_vulkan_wait_idle         (render_vulkan_t* render);
render_result_t render_vulkan_view_create       (render_vulkan_t* render, const render_view_create_info_t* create_info, render_view_id_t* out_view_id);
void            render_vulkan_view_destroy      (render_vulkan_t* render, render_view_id_t view_id);
render_result_t render_vulkan_script_create     (render_vulkan_t* render, const render_script_create_info_t* create_info, render_script_id_t* out_script_ud);
void            render_vulkan_script_destroy    (render_vulkan_t* render, render_script_id_t script_id);
render_result_t render_vulkan_texture_create    (render_vulkan_t* render, const texture_data_t* texture_data, render_texture_id_t* out_texture_id);
void            render_vulkan_texture_destroy   (render_vulkan_t* render, render_texture_id_t texture_id);
render_result_t render_vulkan_shader_create     (render_vulkan_t* render, const shader_data_t* shader_data, render_shader_id_t* out_shader_id);
void            render_vulkan_shader_destroy    (render_vulkan_t* render, render_shader_id_t shader_id);
render_result_t render_vulkan_material_create   (render_vulkan_t* render, const material_data_t* material_data, render_material_id_t* out_material_id);
void            render_vulkan_material_destroy  (render_vulkan_t* render, render_material_id_t material_id);
render_result_t render_vulkan_mesh_create       (render_vulkan_t* render, const mesh_data_t* mesh_data, render_mesh_id_t* out_mesh_id);
void            render_vulkan_mesh_destroy      (render_vulkan_t* render, render_mesh_id_t mesh_id);
render_result_t render_vulkan_instance_create   (render_vulkan_t* render, const render_instance_create_info_t* create_info, render_instance_id_t* out_instance_id);
void            render_vulkan_instance_destroy  (render_vulkan_t* render, render_instance_id_t instance_id);
render_result_t render_vulkan_instance_set_data (render_vulkan_t* render, size_t num_instances, render_instance_id_t* instance_ids, render_instance_data_t* instance_data);
void            render_vulkan_kick_render       (render_vulkan_t* render, render_view_id_t view_id, render_script_id_t script_id); // TODO: multiple views (stereo, shadow, etc)
void            render_vulkan_kick_upload       (render_vulkan_t* render);
#endif

#if defined(FAMILY_WINDOWS)
struct render_dx12_t;

render_result_t render_dx12_create            (const render_create_info_t* create_info, render_dx12_t** out_render);
void            render_dx12_destroy           (render_dx12_t* render);
void            render_dx12_wait_idle         (render_dx12_t* render);
render_result_t render_dx12_view_create       (render_dx12_t* render, const render_view_create_info_t* create_info, render_view_id_t* out_view_id);
void            render_dx12_view_destroy      (render_dx12_t* render, render_view_id_t view_id);
render_result_t render_dx12_script_create     (render_dx12_t* render, const render_script_create_info_t* create_info, render_script_id_t* out_script_ud);
void            render_dx12_script_destroy    (render_dx12_t* render, render_script_id_t script_id);
render_result_t render_dx12_texture_create    (render_dx12_t* render, const texture_data_t* texture_data, render_texture_id_t* out_texture_id);
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
#endif

#if defined(PLATFORM_OSX)
struct render_metal_t;

render_result_t render_metal_create            (const render_create_info_t* create_info, render_metal_t** out_render);
void            render_metal_destroy           (render_metal_t* render);
void            render_metal_wait_idle         (render_metal_t* render);
render_result_t render_metal_view_create       (render_metal_t* render, const render_view_create_info_t* create_info, render_view_id_t* out_view_id);
void            render_metal_view_destroy      (render_metal_t* render, render_view_id_t view_id);
render_result_t render_metal_script_create     (render_metal_t* render, const render_script_create_info_t* create_info, render_script_id_t* out_script_ud);
void            render_metal_script_destroy    (render_metal_t* render, render_script_id_t script_id);
render_result_t render_metal_texture_create    (render_metal_t* render, const texture_data_t* texture_data, render_texture_id_t* out_texture_id);
void            render_metal_texture_destroy   (render_metal_t* render, render_texture_id_t texture_id);
render_result_t render_metal_shader_create     (render_metal_t* render, const shader_data_t* shader_data, render_shader_id_t* out_shader_id);
void            render_metal_shader_destroy    (render_metal_t* render, render_shader_id_t shader_id);
render_result_t render_metal_material_create   (render_metal_t* render, const material_data_t* material_data, render_material_id_t* out_material_id);
void            render_metal_material_destroy  (render_metal_t* render, render_material_id_t material_id);
render_result_t render_metal_mesh_create       (render_metal_t* render, const mesh_data_t* mesh_data, render_mesh_id_t* out_mesh_id);
void            render_metal_mesh_destroy      (render_metal_t* render, render_mesh_id_t mesh_id);
render_result_t render_metal_instance_create   (render_metal_t* render, const render_instance_create_info_t* create_info, render_instance_id_t* out_instance_id);
void            render_metal_instance_destroy  (render_metal_t* render, render_instance_id_t instance_id);
render_result_t render_metal_instance_set_data (render_metal_t* render, size_t num_instances, render_instance_id_t* instance_ids, render_instance_data_t* instance_data);
void            render_metal_kick_render       (render_metal_t* render, render_view_id_t view_id, render_script_id_t script_id); // TODO: multiple views (stereo, shadow, etc)
void            render_metal_kick_upload       (render_metal_t* render);
#endif