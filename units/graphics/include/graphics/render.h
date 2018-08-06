#pragma once

#include <stdint.h>
#include <stdlib.h>

/******************************************************************************\
*
*  Forward declares
*
\******************************************************************************/

struct allocator_t;
struct mesh_data_t;
struct shader_data_t;
struct texture_data_t;
struct material_data_t;

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
	RENDER_RESULT_BACKEND_NOT_SUPPORTED,
} render_result_t;

typedef enum render_backend_t
{
	RENDER_BACKEND_NULL = 0,
	RENDER_BACKEND_DX12,
	RENDER_BACKEND_VULKAN,
	RENDER_BACKEND_METAL,
} render_backend_t;

typedef enum render_format_t
{
	RENDER_FORMAT_
} render_format_t;

typedef enum render_target_size_mode_t {
    RENDER_TARGET_SIZE_MODE_BACKBUFFER_RELATIVE = 0,
    RENDER_TARGET_SIZE_MODE_ABSOLUTE,
} render_target_size_mode_t;

typedef enum render_load_op_t {
    RENDER_LOAD_OP_LOAD = 0,
    RENDER_LOAD_OP_CLEAR,
    RENDER_LOAD_OP_DONT_CARE,
} render_load_op_t;

typedef enum render_store_op_t {
    RENDER_STORE_OP_STORE = 0,
    RENDER_STORE_OP_DONT_CARE,
} render_store_op_t;

typedef enum render_command_type_t
{
	RENDER_COMMAND_DRAW = 0,
} render_command_type_t;

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

	render_backend_t preferred_backend;

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
	const char* name;
	render_target_size_mode_t size_mode;
	float width;
	float height;
	uint32_t format; // TODO: enum
} render_target_t;

typedef union render_clear_color_value_t {
    float float32[4];
    int32_t int32[4];
    uint32_t uint32[4];
} render_clear_color_value_t;

typedef struct render_clear_depth_stencil_value_t {
    float depth;
    uint32_t stencil;
} render_clear_depth_stencil_value_t;

typedef union render_clear_value_t {
    render_clear_color_value_t color;
    render_clear_depth_stencil_value_t depth_stencil;
} render_clear_value_t;

typedef struct render_attachment_t
{
	const char* name; // TODO: better identification? render to persistent texture?
    render_load_op_t  load_op;
    render_store_op_t store_op;
	render_clear_value_t clear_value;
} render_attachment_t;

typedef struct render_command_t
{
	render_command_type_t type;
	union
	{
		struct
		{
			char dummy; // currently draws everything...
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

	// TODO: graph instead of linear array of passes?
	size_t num_passes;
	render_pass_t* passes;
} render_script_create_info_t;

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

render_result_t render_texture_create(render_t* render, const texture_data_t* texture_data, render_texture_id_t* out_texture_id);

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
