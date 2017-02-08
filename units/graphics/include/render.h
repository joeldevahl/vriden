#pragma once

#include <allocator.h>

/* external types */
#include <units/graphics/types/mesh.h>
#include <units/graphics/types/shader.h>
#include <units/graphics/types/material.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************\
*
*  General defines
*
\******************************************************************************/

/******************************************************************************\
*
*  Enumerations
*
\******************************************************************************/

typedef enum render_result_t
{
	RENDER_RESULT_OK,
} render_result_t;

typedef enum render_backend_t
{
	RENDER_BACKEND_NULL = 0,
	RENDER_BACKEND_DX12,
} render_backend_t;

/******************************************************************************\
*
*  Internal types
*
\******************************************************************************/

typedef struct render_t render_t;
typedef uint32_t render_view_id_t;
typedef uint32_t render_script_id_t;
typedef uint32_t render_texture_id_t;
typedef uint32_t render_shader_id_t;
typedef uint32_t render_material_id_t;
typedef uint32_t render_mesh_id_t;
typedef uint32_t render_instance_id_t;

/******************************************************************************\
*
*  Structures
*
\******************************************************************************/

typedef struct render_create_info_t
{
	allocator_t* allocator;
	void* window;

	size_t max_textures;
	size_t max_shaders;
	size_t max_materials;
	size_t max_meshes;
	size_t max_instances;
} render_create_info_t;

typedef struct render_view_data_t
{
	float view_mat[16];
	float proj_mat[16];
} render_view_data_t;

typedef struct render_view_create_info_t
{
	render_view_data_t initial_data;
} render_view_create_info_t;

typedef struct render_target_t
{
	uint16_t width;
	uint16_t height;
	uint32_t format; // TODO: enum
	const char* name;
} render_target_t;

typedef struct render_attachment_t
{
	const char* name;
	// TODO: load/store info
} render_attachment_t;

typedef struct render_command_t
{
	uint32_t type;
	union
	{
		struct
		{
			char dummy;
		} draw;
	};
} render_command_t;

typedef struct render_pass_t
{
	size_t num_color_attachments;
	render_attachment_t* color_attachments;
	render_attachment_t* depth_stencil_attachment;

	size_t num_commands;
	render_command_t* commands;
} render_pass_t;

typedef struct render_script_create_info_t
{
	size_t num_transient_targets;
	render_target_t* transient_targets;

	size_t num_passes;
	render_pass_t* passes;
} render_script_create_info_t;

typedef struct render_texture_create_info_t
{
	uint16_t width;
	uint16_t height;
	const void* data; // TODO: better initialization

} render_texture_create_info_t;

typedef struct render_instance_data_t
{
	float transform[16];
} render_instance_data_t;

typedef struct render_instance_create_info_t
{
	render_mesh_id_t mesh_id;
	render_material_id_t material_id;
	render_instance_data_t initial_data;
} render_instance_create_info_t;


/******************************************************************************\
*
*  Render operations
*
\******************************************************************************/

render_result_t render_create(const render_create_info_t* create_info, render_t** out_render);

void render_destroy(render_t* render);

void render_wait_idle(render_t* render);

render_result_t render_view_create(render_t* render, const render_view_create_info_t* create_info, render_view_id_t* out_view_id);

void render_view_destroy(render_t* render, render_view_id_t view_id);

render_result_t render_script_create(render_t* render, const render_script_create_info_t* create_info, render_script_id_t* out_script_ud);

void render_script_destroy(render_t* render, render_script_id_t script_id);

render_result_t render_texture_create(render_t* render, const render_texture_create_info_t* create_info, render_texture_id_t* out_texture_id);

void render_texture_destroy(render_t* render, render_texture_id_t texture_id);

render_result_t render_shader_create(render_t* render, const shader_data_t* shader_data, render_shader_id_t* out_shader_id);

void render_shader_destroy(render_t* render, render_shader_id_t shader_id);

render_result_t render_material_create(render_t* render, const material_data_t* material_data, render_material_id_t* out_material_id);

void render_material_destroy(render_t* render, render_material_id_t material_id);

render_result_t render_mesh_create(render_t* render, const mesh_data_t* mesh_data, render_mesh_id_t* out_mesh_id);

void render_mesh_destroy(render_t* render, render_mesh_id_t mesh_id);

render_result_t render_instance_create(render_t* render, const render_instance_create_info_t* create_info, render_instance_id_t* out_instance_id);

void render_instance_destroy(render_t* render, render_instance_id_t instance_id);

render_result_t render_instance_set_data(render_t* render, size_t num_instances, render_instance_id_t* instance_ids, render_instance_data_t* instance_data);

void render_kick_render(render_t* render, render_view_id_t view_id, render_script_id_t script_id); // TODO: multiple views (stereo, shadow, etc)

void render_kick_upload(render_t* render);

#ifdef __cplusplus
}
#endif