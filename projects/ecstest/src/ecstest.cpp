#include <application/application.h>
#include <application/window.h>
#include <foundation/job_system.h>
#include <foundation/array.h>
#include <foundation/cobjpool.h>
#include <game/ecs.h>

#include <windows.h>
#include <stdint.h>

static uint64_t PAGE_SIZE = 4 * 1024 * 1024; // 4KiB by default

void* virtual_reserve(uint64_t size)
{
	return VirtualAlloc(NULL, size, MEM_RESERVE, 0);
}

void virtual_commit(void* addr, uint64_t size)
{
	VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE);
}

void* virtual_alloc(uint64_t size)
{
	return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void virtual_release(void* addr, uint64_t size)
{
	VirtualAlloc(addr, size, MEM_RELEASE, 0);
}

void* eop_malloc(allocator_t* allocator, size_t count, size_t size, size_t align, const char* file, int line)
{
	uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	char* base = (char*)virtual_alloc(pages * PAGE_SIZE);
	uint64_t offset = pages * PAGE_SIZE - size;
	return base + offset;
}

void* eop_realloc(allocator_t* allocator, void* memory, size_t count, size_t size, size_t align, const char* file, int line)
{
	BREAKPOINT();
	// not implemented
	return nullptr;
}

void eop_free(allocator_t* allocator, void* memory, const char* file, int line)
{
	// TODO: need a start header of allocation to do virtual_release
}

allocator_t eop_allocator = {
	eop_malloc,
	eop_realloc,
	eop_free,
};

struct position_component_t
{
	float mat[16];
};

int application_main(application_t* application)
{
	allocator_t* allocator = &eop_allocator;

	window_create_params_t window_create_params = {};
	window_create_params.allocator = allocator;
	window_create_params.width = 1280;
	window_create_params.height = 720;
	window_create_params.name = "ecstest";
	window_t* window = window_create(&window_create_params);

	job_system_create_params_t job_system_create_params = {};
	job_system_create_params.alloc = allocator;
	job_system_create_params.num_threads = 8; // TODO
	job_system_create_params.max_cached_functions = 1024;
	job_system_create_params.worker_thread_temp_size = 4 * 1024 * 1024; // 4 MiB temp size per thread
	job_system_create_params.max_job_argument_size = 1024; // Each arg can be max 1KiB i size
	job_system_create_params.job_argument_alignment = 16; // And will be 16 byte aligned
	job_system_t* job_system = job_system_create(&job_system_create_params);

	job_system_result_t job_res = job_system_load_bundle(job_system, "ecstest_jobs" CONFIG_SUFFIX DYNAMIC_LIBRARY_EXTENSION);

	static const uint32_t NUM_ENTITIES = 1024;

	ecs_create_info_t ecs_create_info = {};
	ecs_create_info.allocator = allocator;
	ecs_create_info.max_entities = NUM_ENTITIES;
	ecs_create_info.max_component_types = 1;
	ecs_t* ecs = nullptr;
	ecs_result_t ecs_res = ecs_create(&ecs_create_info, &ecs);
	(void)ecs_res;

	component_type_create_info_t position_component_create_info = {};
	position_component_create_info.component_size = sizeof(position_component_t);
	position_component_create_info.max_components = NUM_ENTITIES;
	component_type_id_t position_type_id = {};
	ecs_res = ecs_register_component_type(ecs, &position_component_create_info, &position_type_id);

	entity_id_t entities[NUM_ENTITIES];
	for (uint32_t i = 0; i < NUM_ENTITIES; ++i)
	{
		position_component_t position_data = {};

		component_data_t component_data = {};
		component_data.type = position_type_id;
		component_data.data = &position_data;

		entity_create_info_t entity_create_info = {};
		entity_create_info.num_components = 1;
		entity_create_info.component_datas = &component_data;

		ecs_res = ecs_entity_create(ecs, &entity_create_info, &entities[i]);
	}

	while (application_is_running(application))
	{
		application_update(application);
	}
	
	for (uint32_t i = 0; i < NUM_ENTITIES; ++i)
	{
		ecs_entity_destroy(ecs, entities[i]);
	}

	ecs_destroy(ecs);

	job_system_destroy(job_system);

	window_destroy(window);

	return 0;
}
