#pragma once

#include <stdint.h>

struct job_system_t;

struct job_system_create_params_t
{
	struct allocator_t* alloc;
	uint8_t num_threads;
	uint8_t num_queue_slots;
	uint8_t max_bundles;
};

typedef void (*job_function_t)(void*);

struct job_ptr_descriptor_t
{
	job_function_t function;
	const char* name;
};

enum job_system_result_t
{
	JOB_SYSTEM_OK = 0,
	JOB_SYSTEM_COULD_NOT_LOAD_BUNDLE,
	JOB_SYSTEM_COULD_NOT_FIND_JOB_TABLE,
	JOB_SYSTEM_JOB_NOT_FOUND,
};

job_system_t* job_system_create(const job_system_create_params_t* params);
void job_system_destroy(job_system_t* system);

job_system_result_t job_system_load_bundle(job_system_t* system, const char* name);

job_system_result_t job_system_kick(job_system_t* system, uint32_t name, void* arg, size_t arg_size);
job_system_result_t job_system_kick_ptr(job_system_t* system, job_function_t function, void* arg, size_t arg_size);
