#pragma once

#include "allocator.h"

enum vfs_result_t
{
	VFS_RESULT_OK = 0,
	VFS_RESULT_PENDING,
	VFS_RESULT_FILE_NOT_FOUND,
	VFS_RESULT_TOO_MANY_REQUESTS,
	VFS_RESULT_REQUEST_TOO_BIG,
	VFS_RESULT_TOO_MANY_MOUNTS,
};

struct vfs_t;
typedef uint16_t vfs_request_t;

typedef vfs_result_t (*vfs_read_func_t)(void* context, const char* filename, allocator_t* allocator, void** out_data, size_t* out_size);

struct vfs_create_params_t
{
	allocator_t* allocator;
	size_t max_mounts;
	size_t max_requests;
	size_t buffer_size;
};

vfs_t* vfs_create(const vfs_create_params_t* params);

void vfs_destroy(vfs_t* vfs);

vfs_result_t vfs_add_mount(vfs_t* vfs, vfs_read_func_t read_func, void* context);

vfs_result_t vfs_begin_request(vfs_t* vfs, const char* filename, vfs_request_t* out_request);

vfs_result_t vfs_request_status(vfs_t* vfs, vfs_request_t request);

vfs_result_t vfs_request_wait_not_pending(vfs_t* vfs, vfs_request_t request);

vfs_result_t vfs_request_data(vfs_t* vfs, vfs_request_t request, void** out_data, size_t* out_size);

vfs_result_t vfs_end_request(vfs_t* vfs, vfs_request_t request);

vfs_result_t vfs_sync_read(vfs_t* vfs, allocator_t* allocator, const char* filename, void** out_data, size_t* out_size);
