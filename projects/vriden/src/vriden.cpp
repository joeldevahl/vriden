#include <application/application.h>
#include <application/window.h>
#include <foundation/array.h>
#include <foundation/assert.h>
#include <foundation/hash.h>
#include <foundation/job_system.h>
#include <foundation/resource_cache.h>
#include <foundation/time.h>
#include <foundation/vfs.h>
#include <foundation/vfs_mount_fs.h>
#include <graphics/render.h>

#include <dl/dl.h>

#include <getopt/getopt.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>

#include "resource_context.h"

#define DATA_PATH "local/build/" PLATFORM_STRING "/"
#define MAX_ENTITIES 10000

#define ERROR_AND_QUIT(fmt, ...) { fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__); return 0x0; }
#define ERROR_AND_FAIL(fmt, ...) { fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__); return 1; }

void print_help(getopt_context_t* ctx)
{
	char buffer[2048];
	printf("usage: shaderc [options] file\n\n");
	printf("%s", getopt_create_help_string(ctx, buffer, sizeof(buffer)));
}

struct dummy_job_params_t
{
	volatile uint32_t* job_done_ptr;
};

void mesh_register_creator(resource_context_t* resource_context);
void shader_register_creator(resource_context_t* resource_context);
void texture_register_creator(resource_context_t* resource_context);
void material_register_creator(resource_context_t* resource_context);

int application_main(application_t* application)
{
#if defined(FAMILY_WINDOWS)
	int render_backend = RENDER_BACKEND_DX12;
#elif defined(PLATFORM_OSX)
	int render_backend = RENDER_BACKEND_METAL;
#else
#error not implemented for this platform
#endif

	static const getopt_option_t option_list[] =
	{
		{ "help", 'h', GETOPT_OPTION_TYPE_NO_ARG, 0x0, 'h', "displays this message", 0x0 },
		{ "render-backend-null", 0, GETOPT_OPTION_TYPE_FLAG_SET, &render_backend, RENDER_BACKEND_NULL, "set render backend to null",  0x0 },
#if defined(FAMILY_WINDOWS)
		{ "render-backend-dx12", 0, GETOPT_OPTION_TYPE_FLAG_SET, &render_backend, RENDER_BACKEND_DX12, "set render backend to dx12",  0x0 },
		{ "render-backend-vulkan", 0, GETOPT_OPTION_TYPE_FLAG_SET, &render_backend, RENDER_BACKEND_VULKAN, "set render backend to vulkan",  0x0 },
#endif
#if defined(PLATFORM_OSX)
		{ "render-backend-metal", 0, GETOPT_OPTION_TYPE_FLAG_SET, &render_backend, RENDER_BACKEND_METAL, "set render backend to metal",  0x0 },
#endif
		GETOPT_OPTIONS_END
	};

	getopt_context_t go_ctx;
	getopt_create_context(&go_ctx, application_get_argc(application), application_get_argv(application), option_list);

	int opt;
	while( (opt = getopt_next( &go_ctx ) ) != -1 )
	{
		switch(opt)
		{
			case 'h': print_help(&go_ctx); return 0;
			case 0: break; // ignore, flag was set!
		}
	}

	resource_context_t resource_context = {};
	resource_context.application = application;

	window_create_params_t window_create_params;
	window_create_params.allocator = &allocator_malloc;
	window_create_params.application = application;
	window_create_params.width = 1280;
	window_create_params.height = 720;
	window_create_params.name = "vriden";
	window_t* window = window_create(&window_create_params);
	resource_context.window = window;

	job_system_create_params_t job_system_create_params;
	job_system_create_params.alloc = &allocator_malloc;
	job_system_create_params.num_threads = 8; // TODO
	job_system_create_params.max_cached_functions = 1024;
	job_system_create_params.worker_thread_temp_size = 4 * 1024 * 1024; // 4 MiB temp size per thread
	job_system_create_params.max_job_argument_size = 1024; // Each arg can be max 1KiB i size
	job_system_create_params.job_argument_alignment = 16; // And will be 16 byte aligned
	job_system_t* job_system = job_system_create(&job_system_create_params);
	resource_context.job_system = job_system;

	job_system_result_t job_res =  job_system_load_bundle(job_system, "vriden_jobs" CONFIG_SUFFIX DYNAMIC_LIBRARY_EXTENSION);
	(void)job_res;

	vfs_create_params_t vfs_params;
	vfs_params.allocator = &allocator_malloc;
	vfs_params.max_mounts = 4;
	vfs_params.max_requests = 32;
	vfs_params.buffer_size = 16 * 1024 * 1024; // 16 MiB
	vfs_t* vfs = vfs_create(&vfs_params);
	resource_context.vfs = vfs;

	vfs_mount_fs_t* vfs_fs_data = vfs_mount_fs_create(&allocator_malloc, DATA_PATH);
	vfs_result_t vfs_res = vfs_add_mount(vfs, vfs_mount_fs_read_func, vfs_fs_data);
	ASSERT(vfs_res == VFS_RESULT_OK, "failed to add vfs mount");

	dl_ctx_t dl_ctx;
	dl_create_params_t dl_create_params;
	DL_CREATE_PARAMS_SET_DEFAULT(dl_create_params);
	dl_error_t err = dl_context_create(&dl_ctx, &dl_create_params);
	(void)err;
	resource_context.dl_ctx = dl_ctx;

	resource_cache_create_params_t resource_cache_params;
	resource_cache_params.allocator = &allocator_malloc;
	resource_cache_params.vfs = vfs;
	resource_cache_params.max_resources = 1024;
	resource_cache_params.max_resource_handles = 4096;
	resource_cache_params.max_creators = 16;
	resource_cache_params.max_callbacks = 16;
	resource_cache_t* resource_cache = resource_cache_create(&resource_cache_params);
	resource_context.resource_cache = resource_cache;

	render_create_info_t render_create_info;
	render_create_info.allocator = &allocator_malloc;
	render_create_info.window = window_get_platform_handle(window);
	render_create_info.preferred_backend = (render_backend_t)render_backend;
	render_create_info.max_textures = 1;
	render_create_info.max_shaders = 1;
	render_create_info.max_materials = 1;
	render_create_info.max_meshes = 1;
	render_create_info.max_instances = MAX_ENTITIES;
	render_t* render;
	render_result_t render_res = render_create(&render_create_info, &render);
	ASSERT(render_res == RENDER_RESULT_OK, "failed create render");
	resource_context.render = render;

	render_view_create_info_t view_create_info;
	glm::mat4x4 projection_matrix = glm::perspective(glm::radians(45.0f), 1280.0f/720.0f, 0.1f, 1000.0f);
	glm::mat4x4 view_matrix = glm::lookAt(glm::vec3(0.0f, 0.0f, 150.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	memcpy(view_create_info.initial_data.proj_mat, glm::value_ptr(projection_matrix), sizeof(view_create_info.initial_data.proj_mat));
	memcpy(view_create_info.initial_data.view_mat, glm::value_ptr(view_matrix), sizeof(view_create_info.initial_data.view_mat));
	render_view_id_t view_id;
	render_res = render_view_create(render, &view_create_info, &view_id);
	ASSERT(render_res == RENDER_RESULT_OK, "failed create view");

	render_target_t transient_targets[] =
	{
		{
			"depth buffer",
			RENDER_TARGET_SIZE_MODE_BACKBUFFER_RELATIVE,
			1.0f, 1.0f,
			0, // TODO: format?
		},
	};

	render_attachment_t color_attachments[] =
	{
		{
			"back buffer",
			RENDER_LOAD_OP_CLEAR,
			RENDER_STORE_OP_STORE,
			// initialize color below
		},
	};
	color_attachments[0].clear_value.color.float32[0] = 0.3f;
	color_attachments[0].clear_value.color.float32[1] = 0.3f;
	color_attachments[0].clear_value.color.float32[2] = 0.6f;
	color_attachments[0].clear_value.color.float32[3] = 1.0f;

	render_attachment_t depth_attachment =
	{
		"depth buffer",
		RENDER_LOAD_OP_CLEAR,
		RENDER_STORE_OP_STORE,
		// initialize depth/stencil below
	};
	depth_attachment.clear_value.depth_stencil.depth = 1.0f;
	depth_attachment.clear_value.depth_stencil.stencil = 0;

	render_command_t commands[1];
	commands[0].type = RENDER_COMMAND_DRAW;
	commands[0].draw.dummy = 0; // TODO

	render_pass_t passes[] =
	{
		{ ARRAY_LENGTH(color_attachments), color_attachments, &depth_attachment, ARRAY_LENGTH(commands), commands },
	};

	render_script_create_info_t script_create_info;
	script_create_info.num_transient_targets = ARRAY_LENGTH(transient_targets);
	script_create_info.transient_targets = transient_targets;
	script_create_info.num_passes = ARRAY_LENGTH(passes);
	script_create_info.passes = passes;
	render_script_id_t script_id;
	render_res = render_script_create(render, &script_create_info, &script_id);
	ASSERT(render_res == RENDER_RESULT_OK, "failed create script");

	mesh_register_creator(&resource_context);
	shader_register_creator(&resource_context);
	texture_register_creator(&resource_context);
	material_register_creator(&resource_context);

	char* precache_text = nullptr;
	size_t precache_size = 0;
	vfs_sync_read(vfs, &allocator_malloc, "vriden_precache.txt", (void**)&precache_text, &precache_size);
	precache_text[precache_size - 1] = '\0';

	scoped_array_t<resource_handle_t> handles(&allocator_malloc);
	char* precache_name = strtok(precache_text, "\r\n");
	while (precache_name != nullptr)
	{
		const char* type_str = strrchr(precache_name, '.') + 1;
		if (strcmp(type_str, "vl") != 0) // TODO: remove this early out and select what to precache better
		{
			uint32_t name_hash = hash_string(precache_name);
			uint32_t type_hash = hash_string(type_str);

			vfs_request_t request;
			vfs_res = vfs_begin_request(vfs, precache_name, &request);
			ASSERT(vfs_res == VFS_RESULT_OK, "vfs failed to begin request");

			vfs_res = vfs_request_wait_not_pending(vfs, request);
			ASSERT(vfs_res == VFS_RESULT_OK, "vfs failed request");

			uint8_t* data = NULL;
			size_t size = 0;
			vfs_res = vfs_request_data(vfs, request, (void**)&data, &size);
			ASSERT(vfs_res == VFS_RESULT_OK, "vfs failed request");

			ASSERT(data, "No data loaded");

			resource_handle_t handle;
			resource_cache_result_t resource_res = resource_cache_create_resource(resource_cache, name_hash, type_hash, data, size, &handle);
			ASSERT(resource_res == RESOURCE_CACHE_RESULT_OK);
			handles.grow_and_append(handle);

			vfs_res = vfs_end_request(vfs, request);
			ASSERT(vfs_res == VFS_RESULT_OK, "vfs failed to end request");
		}
		precache_name = strtok(nullptr, "\r\n");
	}
	ALLOCATOR_FREE(&allocator_malloc, precache_text);

	render_kick_upload(render);

	resource_handle_t mesh_handle;
	resource_cache_result_t resource_res = resource_cache_get_by_name(resource_cache, "projects/vriden/data/meshes/plane_xy.mesh", &mesh_handle);
	ASSERT(resource_res == RESOURCE_CACHE_RESULT_OK);
	void* mesh_ptr;
	resource_cache_handle_to_pointer(resource_cache, mesh_handle, &mesh_ptr);
	render_mesh_id_t mesh_id = (render_mesh_id_t)(uintptr_t)mesh_ptr;

	resource_handle_t material_handle;
	resource_res = resource_cache_get_by_name(resource_cache, "projects/vriden/data/materials/default.material", &material_handle);
	ASSERT(resource_res == RESOURCE_CACHE_RESULT_OK);
	void* material_ptr;
	resource_cache_handle_to_pointer(resource_cache, material_handle, &material_ptr);
	render_material_id_t material_id = (render_material_id_t)(uintptr_t)material_ptr;

	render_instance_id_t instance_id[MAX_ENTITIES];
	render_instance_create_info_t instance_create_info =
	{
		mesh_id,
		material_id,
		{
			{ 0 },
		},
	};
	for (size_t i = 0; i < MAX_ENTITIES; ++i)
	{
		float x = (float)(i % 100) - 50;
		float y = (float)(i / 100) - 50;
		glm::mat4x4 translation = glm::translate(glm::vec3(x, y, 0.0f));
		glm::mat4x4 rotx = glm::rotate(x * 0.01f, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4x4 roty = glm::rotate(y * 0.01f, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4x4 mat = translation * rotx * roty;
		memcpy(instance_create_info.initial_data.transform, glm::value_ptr(mat), sizeof(instance_create_info.initial_data.transform));
		render_res = render_instance_create(render, &instance_create_info, &instance_id[i]);
		ASSERT(render_res == RENDER_RESULT_OK, "failed create render instance");
	}

	uint64_t freq = time_frequency();
	uint64_t start = time_current();
	render_instance_data_t instance_data[MAX_ENTITIES];
	while (application_is_running(application))
	{
		uint64_t curr = time_current();
		uint64_t time = curr - start;
		float t = (float)time / (float)freq;
		application_update(application);

		for (size_t i = 0; i < MAX_ENTITIES; ++i)
		{
			float x = (float)(i % 100) - 50;
			float y = (float)(i / 100) - 50;
			glm::mat4x4 translation = glm::translate(glm::vec3(x, y, 0.0f));
			glm::mat4x4 rotx = glm::rotate((x + t) * 0.01f, glm::vec3(1.0f, 0.0f, 0.0f));
			glm::mat4x4 roty = glm::rotate((y + t) * 0.01f, glm::vec3(0.0f, 1.0f, 0.0f));
			glm::mat4x4 mat = translation * rotx * roty;
			memcpy(instance_data[i].transform, glm::value_ptr(mat), sizeof(instance_data[i].transform));
		}

		render_instance_set_data(render, MAX_ENTITIES, instance_id, instance_data);
		
		render_kick_render(render, view_id, script_id);
	}

	render_wait_idle(render);
	render_script_destroy(render, script_id);
	render_view_destroy(render, view_id);

	for (size_t i = 0; i < MAX_ENTITIES; ++i)
	{
		render_instance_destroy(render, instance_id[i]);
	}

	resource_cache_release_handle(resource_cache, mesh_handle);
	resource_cache_release_handle(resource_cache, material_handle);
	for (auto handle : handles)
	{
		resource_cache_release_handle(resource_cache, handle);
	}

	render_destroy(render);

	resource_cache_destroy(resource_cache);

	dl_context_destroy(dl_ctx);

	vfs_mount_fs_destroy(vfs_fs_data);
	vfs_destroy(vfs);

	job_system_destroy(job_system);

	window_destroy(window);

	return 0;
}
