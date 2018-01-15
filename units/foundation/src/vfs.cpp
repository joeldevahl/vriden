#include <foundation/array.h>
#include <foundation/objpool.h>
#include <foundation/circular_queue.h>

#include <foundation/vfs.h>

#include <thread>
#include <mutex>
#include <condition_variable>

typedef struct vfs_mount_t
{
	void* context;
	vfs_read_func_t read;
} vfs_mount_t;

struct vfs_request_data_t
{
	char filename[1024];
	volatile vfs_result_t status;
	void* data;
	size_t size;
};

struct vfs_t
{
	allocator_t* allocator;
	volatile bool thread_exit;

	array_t<vfs_mount_t> mounts;

	objpool_t<vfs_request_data_t, vfs_request_t> request_pool;
	circular_queue_t<vfs_request_t> request_queue;

	std::thread thread;
	std::mutex mutex;
	std::condition_variable cond;
};

static void vfs_read_request(vfs_t* vfs, allocator_t* allocator, vfs_request_data_t* request)
{
	(void)allocator;
	for(size_t i_mount = 0; i_mount < vfs->mounts.length(); ++i_mount)
	{
		vfs_mount_t* mount = &vfs->mounts[i_mount];
		vfs_result_t status = mount->read(mount->context, request->filename, vfs->allocator, &request->data, &request->size);
		if(status == VFS_RESULT_OK || status == VFS_RESULT_REQUEST_TOO_BIG)
		{
			request->status = status;
			return;
		}
	}

	request->status = VFS_RESULT_FILE_NOT_FOUND;
}

static void vfs_thread(vfs_t* vfs)
{
	while(!vfs->thread_exit)
	{

		vfs_request_data_t* request = nullptr;
		{
			std::unique_lock<std::mutex> lock(vfs->mutex);

			if (vfs->request_queue.empty())
			{
				vfs->cond.wait(lock);
			}

			if (vfs->request_queue.any())
			{
				vfs_request_t id = vfs->request_queue.get();
				request = vfs->request_pool.handle_to_pointer(id);
			}
		}

		if (request != nullptr)
		{
			vfs_read_request(vfs, vfs->allocator, request);
		}
	}
}

vfs_t* vfs_create(const vfs_create_params_t* params)
{
	vfs_t* vfs = ALLOCATOR_NEW(params->allocator, vfs_t);
	vfs->allocator = params->allocator;
	vfs->thread_exit = false;

	vfs->mounts.create(params->allocator, params->max_mounts);
	vfs->request_pool.create(params->allocator, params->max_requests);
	vfs->request_queue.create(params->allocator, params->max_requests);

	vfs->thread = std::thread(vfs_thread, vfs);

	return vfs;
}

void vfs_destroy(vfs_t* vfs)
{
	vfs->thread_exit = true;
	vfs->cond.notify_all();
	vfs->thread.join();

	vfs->request_queue.destroy(vfs->allocator);
	vfs->request_pool.destroy(vfs->allocator);
	vfs->mounts.destroy(vfs->allocator);

	ALLOCATOR_DELETE(vfs->allocator, vfs_t, vfs);
}

vfs_result_t vfs_add_mount(vfs_t* vfs, vfs_read_func_t read_func, void* context)
{
	if(vfs->mounts.full())
		return VFS_RESULT_TOO_MANY_MOUNTS;

	vfs_mount_t mount = {
		context,
		read_func
	};
	vfs->mounts.append(mount);

	return VFS_RESULT_OK;
}

vfs_result_t vfs_begin_request(vfs_t* vfs, const char* filename, vfs_request_t* out_request)
{
	if(vfs->request_pool.full())
		return VFS_RESULT_TOO_MANY_REQUESTS;

	vfs_request_t id = vfs->request_pool.alloc_handle();
	vfs_request_data_t* request = vfs->request_pool.handle_to_pointer(id);
	strncpy(request->filename, filename, ARRAY_LENGTH(request->filename));
	request->status = VFS_RESULT_PENDING;
	request->data = NULL;
	request->size = 0;
	{
		std::lock_guard<std::mutex> guard(vfs->mutex);
		vfs->request_queue.put(id);
	}
	vfs->cond.notify_one();

	*out_request = id;
	return VFS_RESULT_OK;
}

vfs_result_t vfs_request_status(vfs_t* vfs, vfs_request_t request)
{
	vfs_request_data_t* request_ptr = vfs->request_pool.handle_to_pointer(request);
	return request_ptr->status;
}

vfs_result_t vfs_request_wait_not_pending(vfs_t* vfs, vfs_request_t request)
{
	vfs_request_data_t* request_ptr = vfs->request_pool.handle_to_pointer(request);
	while (request_ptr->status == VFS_RESULT_PENDING)
		std::this_thread::yield();
	return request_ptr->status;
}

vfs_result_t vfs_request_data(vfs_t* vfs, vfs_request_t request, void** out_data, size_t* out_size)
{
	vfs_request_data_t* request_ptr = vfs->request_pool.handle_to_pointer(request);
	*out_data = request_ptr->data;
	*out_size = request_ptr->size;
	return request_ptr->status;
}

vfs_result_t vfs_end_request(vfs_t* vfs, vfs_request_t request)
{
	vfs_request_data_t* request_ptr = vfs->request_pool.handle_to_pointer(request);
	ASSERT(request_ptr->status != VFS_RESULT_PENDING, "cannot end pending requests (yet)");
	ALLOCATOR_FREE(vfs->allocator, request_ptr->data);

	std::lock_guard<std::mutex> lock(vfs->mutex);
	vfs->request_pool.free_handle(request);

	return VFS_RESULT_OK;
}

vfs_result_t vfs_sync_read(vfs_t* vfs, allocator_t* allocator, const char* filename, void** out_data, size_t* out_size)
{
	vfs_request_data_t request;
	strncpy(request.filename, filename, ARRAY_LENGTH(request.filename));
	request.status = VFS_RESULT_PENDING;
	request.data = NULL;
	request.size = 0;

	vfs_read_request(vfs, allocator, &request);
	if(request.status == VFS_RESULT_OK)
	{
		*out_data = request.data;
		*out_size = request.size;
	}

	return request.status;
}
