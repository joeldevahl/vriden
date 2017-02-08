#include <foundation/hash.h>
#include <foundation/array.h>
#include <foundation/objpool.h>
#include <foundation/table.h>
#include <foundation/vfs.h>
#include <foundation/resource_cache.h>

struct resource_t
{
	void* resource_data;
	void* private_data;
	uint32_t type_hash;
	uint32_t name_hash;
	uint32_t ref_count;
	uint32_t flags;
};

typedef table_t<uint32_t, resource_creator_t> resource_creator_map_t;
typedef table_t<uint32_t, resource_t> resource_map_t;

struct resource_cache_t
{
	allocator_t* allocator;
	vfs_t* vfs;

	resource_creator_map_t creators;
	resource_map_t resources;
	objpool_t<resource_t*, resource_handle_t> handle_pool;
};

resource_cache_t* resource_cache_create(const resource_cache_create_params_t* params)
{
	resource_cache_t* cache = ALLOCATOR_NEW(params->allocator, resource_cache_t);
	cache->allocator = params->allocator;
	cache->vfs = params->vfs;

	cache->creators.create(params->allocator, params->max_creators, 16);
	cache->resources.create(params->allocator, params->max_resources, 16);
	cache->handle_pool.create(params->allocator, params->max_resource_handles);

	return cache;
}

void resource_cache_destroy(resource_cache_t* cache)
{
	ALLOCATOR_DELETE(cache->allocator, resource_cache_t, cache);
}

resource_cache_result_t resource_cache_add_creator(resource_cache_t* cache, resource_creator_t* creator)
{
	// TODO: make sure type has not been registered
	cache->creators.insert(creator->type_hash, *creator);
	return RESOURCE_CACHE_RESULT_OK;
}

resource_cache_result_t resource_cache_recreate_resource(resource_cache_t* cache, uint32_t name_hash, uint32_t type_hash, void* data, size_t size, resource_handle_t handle)
{
	resource_t* resource = *cache->handle_pool.handle_to_pointer(handle);
	ASSERT(resource == cache->resources.fetch(name_hash));

	resource_creator_t* creator = cache->creators.fetch(type_hash);
	if(creator == nullptr)
		return RESOURCE_CACHE_RESULT_NO_MATCHING_CREATOR;

	void* resource_data = NULL;
	void* private_data = NULL;
	if(!creator->recreate(creator->context, creator->allocator, data, size, resource->resource_data, resource->private_data, &resource_data, &private_data))
		return RESOURCE_CACHE_RESULT_FAILED_TO_RECREATE;

	resource->resource_data = resource_data;
	resource->private_data = private_data;

	// TODO: trigger cascading updates of dependent resources
	return RESOURCE_CACHE_RESULT_OK;
}

resource_cache_result_t resource_cache_create_resource(resource_cache_t* cache, uint32_t name_hash, uint32_t type_hash, void* data, size_t size, resource_handle_t* out_handle)
{
	if(cache->handle_pool.full())
		return RESOURCE_CACHE_RESULT_TOO_MANY_RESOURCE_HANDLES;

	if(cache->resources.has_key(name_hash))
	{
		resource_t* resource = cache->resources.fetch(name_hash);
		resource_handle_t handle = cache->handle_pool.alloc_handle();
		*cache->handle_pool.handle_to_pointer(handle) = resource;
		resource->ref_count += 1;
		*out_handle = handle;
		return resource_cache_recreate_resource(cache, name_hash, type_hash, data, size, handle);
	}

	resource_creator_t* creator = cache->creators.fetch(type_hash);
	if(creator == nullptr)
		return RESOURCE_CACHE_RESULT_NO_MATCHING_CREATOR;

	void* resource_data = NULL;
	void* private_data = NULL;
	if(!creator->create(creator->context, creator->allocator, data, size, &resource_data, &private_data))
		return RESOURCE_CACHE_RESULT_FAILED_TO_CREATE;

	return resource_cache_register_resource(cache, name_hash, type_hash, resource_data, private_data, out_handle);
}

resource_cache_result_t resource_cache_register_resource(resource_cache_t* cache, uint32_t name_hash, uint32_t type_hash, void* resource_data, void* private_data, resource_handle_t* out_handle)
{
	if(cache->handle_pool.full())
		return RESOURCE_CACHE_RESULT_TOO_MANY_RESOURCE_HANDLES;

	// TODO: chack for recreation
	resource_t resource;
	resource.resource_data = resource_data;
	resource.private_data = private_data;
	resource.type_hash = type_hash;
	resource.name_hash = name_hash;
	resource.ref_count = 0;
	resource.flags = 0;

	cache->resources.insert(name_hash, resource);

	return resource_cache_get_by_hash(cache, name_hash, out_handle);
}

static resource_cache_result_t resource_cache_try_get(resource_cache_t* cache, uint32_t name_hash, resource_handle_t* out_handle)
{

	if(cache->resources.has_key(name_hash))
	{
		if(cache->handle_pool.full())
			return RESOURCE_CACHE_RESULT_TOO_MANY_RESOURCE_HANDLES;

		resource_t* resource = cache->resources.fetch(name_hash);
		resource_handle_t handle = cache->handle_pool.alloc_handle();
		*cache->handle_pool.handle_to_pointer(handle) = resource;
		resource->ref_count += 1;
		*out_handle = handle;

		return RESOURCE_CACHE_RESULT_OK;
	}

	return RESOURCE_CACHE_RESULT_NOT_FOUND;
}

resource_cache_result_t resource_cache_get_by_name(resource_cache_t* cache, const char* name, resource_handle_t* out_handle)
{
	uint32_t name_hash = hash_string(name);

	return resource_cache_try_get(cache, name_hash, out_handle);

	// TODO: request stream
	// TODO: add stream in resource
}

resource_cache_result_t resource_cache_get_by_hash(resource_cache_t* cache, uint32_t name_hash, resource_handle_t* out_handle)
{
	return resource_cache_try_get(cache, name_hash, out_handle);

	// TODO: add fail resource
}

resource_cache_result_t resource_cache_release_handle(resource_cache_t* cache, resource_handle_t handle)
{
	resource_t* resource = *cache->handle_pool.handle_to_pointer(handle);
	ASSERT(resource->ref_count > 0, "Double release of resource");

	resource->ref_count -= 1;
	if(resource->ref_count == 0)
	{
		// TODO: put on destroy queue
		resource_creator_t* creator = cache->creators.fetch(resource->type_hash);
		ASSERT(creator != nullptr, "Resource was created, but no mathcing creator was present when destroying");

		creator->destroy(creator->context, creator->allocator, resource->resource_data, resource->private_data);
		cache->resources.remove(resource->name_hash);
	}

	return RESOURCE_CACHE_RESULT_OK;
}

resource_cache_result_t resource_cache_handle_handle_to_pointer(resource_cache_t* cache, resource_handle_t handle, void** out_pointer)
{
	resource_t* resource = *cache->handle_pool.handle_to_pointer(handle);
	*out_pointer = resource->resource_data;

	return RESOURCE_CACHE_RESULT_OK;
}

resource_cache_result_t resource_cache_handles_to_pointers(resource_cache_t* cache, resource_handle_t* handles, size_t num_handles, void** out_pointers)
{
	for(size_t i = 0; i < num_handles; ++i)
	{
		resource_t* resource = *cache->handle_pool.handle_to_pointer(handles[i]);
		out_pointers[i] = resource->resource_data;
	}

	return RESOURCE_CACHE_RESULT_OK;
}
