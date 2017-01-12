#include "application.h"
#include "assert.h"
#include "hash.h"
#include "job_system.h"
#include "resource_cache.h"
#include "thread.h"
#include "vfs.h"
#include "vfs_mount_fs.h"
#include "window.h"

struct resource_context_t
{
	struct application_t* application;
	struct window_t* window;
	struct job_system_t* job_system;
	struct vfs_t* vfs;
	struct resource_cache_t* resource_cache;
};

struct dummy_job_params_t
{
	volatile uint32_t* job_done_ptr;
};

int application_main(application_t* application)
{
	resource_context_t resource_context = {};
	resource_context.application = application;

	window_create_params_t window_create_params;
	window_create_params.allocator = &allocator_malloc;
	window_create_params.width = 1280;
	window_create_params.height = 720;
	window_create_params.name = "vriden";
	window_t* window = window_create(&window_create_params);
	resource_context.window = window;

	job_system_create_params_t job_system_create_params;
	job_system_create_params.alloc = &allocator_malloc;
	job_system_create_params.num_threads = 8; // TODO
	job_system_create_params.max_cached_functions = 1;
	job_system_create_params.worker_thread_temp_size = 4 * 1024 * 1024; // 4 MiB temp size per thread
	job_system_create_params.max_job_argument_size = 1024; // Each arg can be max 1KiB i size
	job_system_create_params.job_argument_alignment = 16; // And will be 16 byte aligned
	job_system_t* job_system = job_system_create(&job_system_create_params);
	resource_context.job_system = job_system;

	job_system_result_t job_res =  job_system_load_bundle(job_system, "vriden_jobs" CONFIG_SUFFIX DYNAMIC_LIBRARY_EXTENSION);

	job_cached_function_t* dummy_job = nullptr;
	job_res = job_system_cache_function(job_system, hash_string("dummy_job"), &dummy_job);

	vfs_create_params_t vfs_params;
	vfs_params.allocator = &allocator_malloc;
	vfs_params.max_mounts = 4;
	vfs_params.max_requests = 32;
	vfs_params.buffer_size = 16 * 1024 * 1024; // 16 MiB
	vfs_t* vfs = vfs_create(&vfs_params);
	resource_context.vfs = vfs;

	vfs_mount_fs_t* vfs_fs_data = vfs_mount_fs_create(&allocator_malloc, "./");
	vfs_result_t vfs_res = vfs_add_mount(vfs, vfs_mount_fs_read_func, vfs_fs_data);
	ASSERT(vfs_res == VFS_RESULT_OK, "failed to add vfs mount");

	resource_cache_create_params_t resource_cache_params;
	resource_cache_params.allocator = &allocator_malloc;
	resource_cache_params.vfs = vfs;
	resource_cache_params.max_resources = 1024;
	resource_cache_params.max_resource_handles = 4096;
	resource_cache_params.max_creators = 16;
	resource_cache_params.max_callbacks = 16;
	resource_cache_t* resource_cache = resource_cache_create(&resource_cache_params);
	resource_context.resource_cache = resource_cache;

	while (application_is_running(application))
	{
		application_update(application);

		job_event_t* event = nullptr;
		job_system_kick(job_system, dummy_job, 1024, nullptr, 0, nullptr, &event);
		job_system_wait_release_event(job_system, event);
	}

	resource_cache_destroy(resource_cache);

	vfs_mount_fs_destroy(vfs_fs_data);
	vfs_destroy(vfs);

	job_system_release_cached_function(job_system, dummy_job);
	job_system_destroy(job_system);

	window_destroy(window);

	return 0;
}
