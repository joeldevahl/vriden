#pragma once

#include "allocator.h"

enum resource_cache_result_t
{
	RESOURCE_CACHE_RESULT_OK = 0,
	RESOURCE_CACHE_RESULT_STREAMING_IN,
	RESOURCE_CACHE_RESULT_NOT_FOUND,
	RESOURCE_CACHE_RESULT_NO_MATCHING_CREATOR,
	RESOURCE_CACHE_RESULT_FAILED_TO_CREATE,
	RESOURCE_CACHE_RESULT_FAILED_TO_RECREATE,
	RESOURCE_CACHE_RESULT_TOO_MANY_RESOURCES,
	RESOURCE_CACHE_RESULT_TOO_MANY_RESOURCE_HANDLES,
	RESOURCE_CACHE_RESULT_TOO_MANY_CREATORS,
	RESOURCE_CACHE_RESULT_TOO_MANY_CALLBACKS,
};

struct resource_cache_t;
typedef uint32_t resource_handle_t;

struct resource_creator_t
{
	bool (*create)(void* context, allocator_t* allocator, void* creation_data, size_t size, void** out_resource_data, void** out_private_data);
	bool (*recreate)(void* context, allocator_t* allocator, void* creation_data, size_t size, void* prev_resource_data, void* prev_private_data, void** out_resource_data, void** out_private_data);
	void (*destroy)(void* context, allocator_t* allocator, void* resource_data, void* private_data);
	allocator_t* allocator;
	void* context;
	uint32_t type_hash;
};

struct resource_cache_create_params_t
{
	allocator_t* allocator;

	struct vfs_t* vfs; // TODO: replace with callbacks

	uint32_t max_resources;
	uint32_t max_resource_handles;
	uint32_t max_creators;
	uint32_t max_callbacks;
};

resource_cache_t* resource_cache_create(const resource_cache_create_params_t* params);

void resource_cache_destroy(resource_cache_t* cache);

resource_cache_result_t resource_cache_add_creator(resource_cache_t* cache, resource_creator_t* creator);

resource_cache_result_t resource_cache_create_resource(resource_cache_t* cache, uint32_t name_hash, uint32_t type_hash, void* data, size_t size, resource_handle_t* out_handle);

resource_cache_result_t resource_cache_recreate_resource(resource_cache_t* cache, uint32_t name_hash, uint32_t type_hash, void* data, size_t size, resource_handle_t handle);

resource_cache_result_t resource_cache_register_resource(resource_cache_t* cache, uint32_t name_hash, uint32_t type_hash, void* resource_data, void* private_data, resource_handle_t* out_handle);

resource_cache_result_t resource_cache_get_by_name(resource_cache_t* cache, const char* name, resource_handle_t* out_handle);

resource_cache_result_t resource_cache_get_by_hash(resource_cache_t* cache, uint32_t name_hash, resource_handle_t* out_handle);

resource_cache_result_t resource_cache_release_handle(resource_cache_t* cache, resource_handle_t handle);

resource_cache_result_t resource_cache_handle_to_pointer(resource_cache_t* cache, resource_handle_t handle, void** out_pointer);

resource_cache_result_t resource_cache_handles_to_pointers(resource_cache_t* cache, resource_handle_t* handles, size_t num_handles, void** out_pointers);
