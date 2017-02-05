#include "vfs_mount_fs.h"
#include "file.h"

#include <cstring>
#include <cstdio>

#ifdef FAMILY_WINDOWS
#	define snprintf _snprintf
#endif

struct vfs_mount_fs_t
{
	char base_path[1024];
	allocator_t* allocator;
};

vfs_result_t vfs_mount_fs_read_func(void* context, const char* filename, allocator_t* allocator, void** out_data, size_t* out_size)
{
	vfs_mount_fs_t* mount = (vfs_mount_fs_t*)context;
	char full_path[2048];
	snprintf(full_path, ARRAY_LENGTH(full_path), "%s/%s", mount->base_path, filename);

	file_t* file = file_open(full_path, FILE_MODE_READ);
	if(file == NULL)
		return VFS_RESULT_FILE_NOT_FOUND;

	size_t size = file_size(file);
	void* data = ALLOCATOR_ALLOC(allocator, size, 16);
	file_read(file, data, size);
	file_close(file);

	*out_data = data;
	*out_size = size;
	return VFS_RESULT_OK;
}

vfs_mount_fs_t* vfs_mount_fs_create(allocator_t* allocator, const char* base_path)
{
	vfs_mount_fs_t* mount = ALLOCATOR_NEW(allocator, vfs_mount_fs_t);
	strncpy(mount->base_path, base_path, ARRAY_LENGTH(mount->base_path));
	mount->allocator = allocator;
	return mount;
}

void vfs_mount_fs_destroy(vfs_mount_fs_t* mount)
{
	ALLOCATOR_DELETE(mount->allocator, vfs_mount_fs_t, mount);
}
