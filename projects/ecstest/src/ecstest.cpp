#include <application/application.h>
#include <application/window.h>
#include <foundation/job_system.h>
#include <foundation/array.h>
#include <foundation/list.h>
#include <foundation/cobjpool.h>
#include <foundation/time.h>
#include <game/ecs.h>

#if defined(FAMILY_WINDOWS)
	#include <windows.h>
#endif
#include <stdint.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

struct allocation_instance_t : list_node_t<allocation_instance_t>
{
	size_t size;
	void* base;
	const char* file;
	int line;
	uint64_t guard;
};

size_t allocator_calc_extra_mem_needed(allocator_t* allocator, size_t alloc_align)
{
	return ALIGN_UP(sizeof(allocation_instance_t), alloc_align);
}

allocation_instance_t* allocator_find_alloc_header(allocator_t* allocator, void* ptr)
{
	return (allocation_instance_t*)((uint8_t*)ptr - sizeof(allocation_instance_t));
}

static uint64_t PAGE_SIZE = 4 * 1024 * 1024; // 4KiB by default

void* virtual_reserve(uint64_t size)
{
#if defined(FAMILY_WINDOWS)
	return VirtualAlloc(NULL, size, MEM_RESERVE, 0);
#else
	return malloc(size);
#endif
}

void virtual_commit(void* addr, uint64_t size)
{
#if defined(FAMILY_WINDOWS)
	VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE);
#else
#endif
}

void* virtual_alloc(uint64_t size)
{
#if defined(FAMILY_WINDOWS)
	return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
	return malloc(size);
#endif
}

void virtual_release(void* addr, uint64_t size)
{
#if defined(FAMILY_WINDOWS)
	VirtualAlloc(addr, size, MEM_RELEASE, 0);
#else
	free(addr);
#endif
}

static linked_list_t<allocation_instance_t> allocation_list;

void* eop_alloc(allocator_t* allocator, size_t count, size_t size, size_t align, const char* file, int line)
{
	size_t extra = allocator_calc_extra_mem_needed(allocator, align);

	uint64_t pages = ALIGN_UP(count * size + extra, PAGE_SIZE) / PAGE_SIZE;
	char* base = (char*)virtual_alloc(pages * PAGE_SIZE);
	uint64_t offset = pages * PAGE_SIZE - count * size;
	void* res = base + offset;

	allocation_instance_t* instance = allocator_find_alloc_header(allocator, res);
	instance->size = count * size;
	instance->base = base;
	instance->file = file;
	instance->line = line;
	instance->guard = 0xF00FD00DF00FD00DULL;

	allocation_list.push_back(instance);

	return res;
}

void eop_free(allocator_t* allocator, void* memory, const char* file, int line)
{
	if (memory)
	{
		allocation_instance_t* instance = allocator_find_alloc_header(allocator, memory);
		ASSERT(instance->guard == 0xF00FD00DF00FD00DULL);
		allocation_list.remove(instance);
		virtual_release(instance->base, instance->size);
	}
}

void* eop_realloc(allocator_t* allocator, void* memory, size_t count, size_t size, size_t align, const char* file, int line)
{
	void* new_mem = eop_alloc(allocator, count, size, align, file, line);
	if (memory)
	{
		allocation_instance_t* instance = allocator_find_alloc_header(allocator, memory);
		memcpy(new_mem, memory, std::min(count * size, instance->size));
		eop_free(allocator, memory, file, line);
	}
	return new_mem;
}

allocator_t eop_allocator = {
	eop_alloc,
	eop_realloc,
	eop_free,
};

struct position_component_t
{
	float transform[16];
};

struct render_component_t
{
	float transform[16];
};

struct position_job_arg_t
{
	position_component_t* positions;
	entity_id_t* eids;
	uint32_t count;
	float time;
};

void position_job_func(job_context_t* context, void* arg)
{
	position_job_arg_t* position_arg = (position_job_arg_t*)arg;
	position_component_t* positions = position_arg->positions;
	entity_id_t* eids = position_arg->eids;
	(void)eids;
	uint32_t count = position_arg->count;
	float t = position_arg->time;

	for (uint32_t i = 0; i < count; ++i)
	{
		float x = (float)(i % 100) - 50;
		float y = (float)(i / 100) - 50;
		glm::mat4x4 translation = glm::translate(glm::vec3(x, y, 0.0f));
		glm::mat4x4 rotx = glm::rotate((x + t) * 0.01f, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4x4 roty = glm::rotate((y + t) * 0.01f, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4x4 mat = translation * rotx * roty;
		memcpy(positions[i].transform, glm::value_ptr(mat), sizeof(positions[i].transform));
	}
}

struct render_job_arg_t
{
	ecs_t* ecs;
	component_type_id_t position_type_id;
	render_component_t* renders;
	entity_id_t* eids;
	uint32_t count;
};

void render_job_func(job_context_t* context, void* arg)
{
	render_job_arg_t* render_arg = (render_job_arg_t*)arg;
	ecs_t* ecs = render_arg->ecs;
	component_type_id_t position_type_id = render_arg->position_type_id;
	render_component_t* renders = render_arg->renders;
	entity_id_t* eids = render_arg->eids;
	uint32_t count = render_arg->count;

	for (uint32_t i = 0; i < count; ++i)
	{
		query_result_t position_res = {};
		ecs_result_t ecs_res = ecs_query_component(ecs, eids[i], position_type_id, &position_res); // TODO: batch get? also thread safe (work graph?)
		ASSERT(ecs_res == ECS_RESULT_OK);
		position_component_t* position = (position_component_t*)position_res.component_data;

		memcpy(renders[i].transform, position->transform, sizeof(renders[i].transform));
	}
}

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
	ecs_create_info.max_component_types = 2;
	ecs_t* ecs = nullptr;
	ecs_result_t ecs_res = ecs_create(&ecs_create_info, &ecs);
	(void)ecs_res;

	component_type_create_info_t position_component_create_info = {};
	position_component_create_info.component_size = sizeof(position_component_t);
	position_component_create_info.max_components = NUM_ENTITIES;
	component_type_id_t position_type_id = {};
	ecs_res = ecs_register_component_type(ecs, &position_component_create_info, &position_type_id);

	component_type_create_info_t render_component_create_info = {};
	render_component_create_info.component_size = sizeof(render_component_t);
	render_component_create_info.max_components = NUM_ENTITIES;
	component_type_id_t render_type_id = {};
	ecs_res = ecs_register_component_type(ecs, &render_component_create_info, &render_type_id);

	entity_id_t entities[NUM_ENTITIES];
	for (uint32_t i = 0; i < NUM_ENTITIES; ++i)
	{
		position_component_t position_data = {};
		render_component_t render_data = {};

		component_data_t component_data[2];
		component_data[0].type = position_type_id;
		component_data[0].data = &position_data;
		component_data[1].type = render_type_id;
		component_data[1].data = &render_data;

		entity_create_info_t entity_create_info = {};
		entity_create_info.num_components = 2;
		entity_create_info.component_datas = component_data;

		ecs_res = ecs_entity_create(ecs, &entity_create_info, &entities[i]);
	}

	uint64_t freq = time_frequency();
	uint64_t start = time_current();
	while (application_is_running(application))
	{
		uint64_t curr = time_current();
		uint64_t time = curr - start;
		float t = (float)time / (float)freq;
		application_update(application);

		query_all_result_t position_res = {};
		ecs_res = ecs_query_all_components(ecs, position_type_id, &position_res); // TODO: bake query into job (or job setup)

		position_job_arg_t position_job_arg = {};
		position_job_arg.positions = (position_component_t*)position_res.component_data;
		position_job_arg.eids = position_res.eids;
		position_job_arg.count = position_res.count;
		position_job_arg.time = t;

		job_event_t* positions_done = nullptr;
		job_res = job_system_acquire_event(job_system, &positions_done);
		ASSERT(job_res == JOB_SYSTEM_OK);
		job_res = job_system_kick_ptr(job_system, position_job_func, 1, &position_job_arg, nullptr, positions_done);
		ASSERT(job_res == JOB_SYSTEM_OK);

		query_all_result_t render_res = {};
		ecs_res = ecs_query_all_components(ecs, render_type_id, &render_res); // TODO: bake query into job (or job setup)

		render_job_arg_t render_job_arg = {};
		render_job_arg.ecs = ecs;
		render_job_arg.position_type_id = position_type_id;
		render_job_arg.renders = (render_component_t*)render_res.component_data;
		render_job_arg.eids = render_res.eids;
		render_job_arg.count = render_res.count;

		job_event_t* render_done = nullptr;
		job_res = job_system_acquire_event(job_system, &render_done);
		ASSERT(job_res == JOB_SYSTEM_OK);
		job_res = job_system_kick_ptr(job_system, render_job_func, 1, &render_job_arg, positions_done, render_done);
		ASSERT(job_res == JOB_SYSTEM_OK);

		job_res = job_system_wait_release_event(job_system, render_done);
		ASSERT(job_res == JOB_SYSTEM_OK);
		job_res = job_system_release_event(job_system, positions_done);
		ASSERT(job_res == JOB_SYSTEM_OK);
	}
	
	for (uint32_t i = 0; i < NUM_ENTITIES; ++i)
	{
		ecs_entity_destroy(ecs, entities[i]);
	}

	ecs_destroy(ecs);

	job_system_destroy(job_system);

	window_destroy(window);

	ASSERT(allocation_list.empty());

	return 0;
}
