#include <application/application.h>
#include <application/window.h>
#include <foundation/job_system.h>
#include <foundation/array.h>
#include <foundation/cobjpool.h>
#include <game/ecs.h>

struct position_component_t
{
	float mat[16];
};

int application_main(application_t* application)
{
	window_create_params_t window_create_params = {};
	window_create_params.allocator = &allocator_malloc;
	window_create_params.width = 1280;
	window_create_params.height = 720;
	window_create_params.name = "ecstest";
	window_t* window = window_create(&window_create_params);

	job_system_create_params_t job_system_create_params = {};
	job_system_create_params.alloc = &allocator_malloc;
	job_system_create_params.num_threads = 8; // TODO
	job_system_create_params.max_cached_functions = 1024;
	job_system_create_params.worker_thread_temp_size = 4 * 1024 * 1024; // 4 MiB temp size per thread
	job_system_create_params.max_job_argument_size = 1024; // Each arg can be max 1KiB i size
	job_system_create_params.job_argument_alignment = 16; // And will be 16 byte aligned
	job_system_t* job_system = job_system_create(&job_system_create_params);

	job_system_result_t job_res =  job_system_load_bundle(job_system, "ecstest_jobs" CONFIG_SUFFIX DYNAMIC_LIBRARY_EXTENSION);

	ecs_create_info_t ecs_create_info = {};
	ecs_create_info.allocator = &allocator_malloc;
	ecs_t* ecs = nullptr;
	ecs_result_t ecs_res = ecs_create(&ecs_create_info, &ecs);
	(void)ecs_res;

	while (application_is_running(application))
	{
		application_update(application);
	}

	ecs_destroy(ecs);

	job_system_destroy(job_system);

	window_destroy(window);

	return 0;
}
