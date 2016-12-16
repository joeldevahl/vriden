#pragma once

#include "vfs.h"

struct vfs_mount_fs_t;

vfs_result_t vfs_mount_fs_read_func(void* context, const char* filename, allocator_t* allocator, void** out_data, size_t* out_size);

vfs_mount_fs_t* vfs_mount_fs_create(allocator_t* allocator, const char* base_path);

void vfs_mount_fs_destroy(vfs_mount_fs_t* mount);
