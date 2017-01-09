#pragma once

#include "allocator.h"

struct job_system_t;
struct job_context_t;
struct job_cached_function_t;
struct job_allocator_mark_t;

struct job_system_create_params_t
{
	struct allocator_t* alloc;
	uint16_t num_threads;
	uint16_t max_bundles;
	uint16_t max_cached_functions;
	uint32_t num_queue_slots;
	size_t worker_thread_temp_size;
	size_t max_job_argument_size;
	size_t job_argument_alignment;
};

typedef void (*job_function_t)(job_context_t* context, void* arg);

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
	JOB_SYSTEM_FUNCTION_NOT_FOUND,
	JOB_SYSTEM_TOO_MANY_CACHED_FUNCTIONS,
	JOB_SYSTEM_ARGUMENT_TOO_BIG,
};

job_system_t* job_system_create(const job_system_create_params_t* params);
void job_system_destroy(job_system_t* system);

job_system_result_t job_system_load_bundle(job_system_t* system, const char* bundle_name);

job_system_result_t job_system_cache_function(job_system_t* system, uint32_t function_name_hash, job_cached_function_t** out_cached_function);
job_system_result_t job_system_release_cached_function(job_system_t* system, job_cached_function_t* cached_function);

job_system_result_t job_system_kick(job_system_t* system, job_cached_function_t* cached_function, void* arg, size_t arg_size);
job_system_result_t job_system_kick_ptr(job_system_t* system, job_function_t function, void* arg, size_t arg_size);

job_system_result_t job_context_get_allocator(job_context_t* context, allocator_t** out_allocator);
job_system_result_t job_context_kick(job_context_t* context, job_cached_function_t* cached_function, void* arg, size_t arg_size);
job_system_result_t job_context_call(job_context_t* context, job_cached_function_t* cached_function, void* arg);
job_system_result_t job_context_kick_ptr(job_context_t* context, job_function_t function, void* arg, size_t arg_size);