#include "application.h"
#include "assert.h"
#include "resource_cache.h"
#include "vfs.h"
#include "vfs_mount_fs.h"
#include "window.h"

struct resource_context_t
{
	struct application_t* application;
	struct window_t* window;
	struct vfs_t* vfs;
	struct resource_cache_t* resource_cache;
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
	}

	resource_cache_destroy(resource_cache);

	vfs_mount_fs_destroy(vfs_fs_data);
	vfs_destroy(vfs);

	window_destroy(window);

	return 0;
}
