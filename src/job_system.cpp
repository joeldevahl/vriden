#include "assert.h"
#include "hash.h"

#include "memory.h"
#include "allocator.h"

#include "array.h"
#include "list.h"
#include "objpool.h"

#include "thread.h"
#include "mutex.h"
#include "job_system.h"

#ifdef FAMILY_WINDOWS
#	define WIN32_LEAN_AND_MEAN
#	define _WIN32_WINNT 0x0600
#	include <windows.h>
#elif defined(FAMILY_UNIX)
#	include <dlfcn.h>
#endif //#if defined(FAMILY_*)

#include <cstdio> // For snprintf
#include <unordered_map>

enum job_command_t
{
	JOB_COMMAND_READY = 0,
	JOB_COMMAND_EXIT,
};

// internal loaded representation
struct job_entry_t
{
	job_function_t function;
	uint32_t name_hash;
	uint32_t bundle_name_hash;
};

struct job_bundle_t
{
#if defined(FAMILY_WINDOWS)
	HMODULE handle;
	char file_name[MAX_PATH]; // Holds temp file name so that file can be deleted later
#elif defined(FAMILY_UNIX)
	void* handle;
#endif //#if defined(FAMILY_*)
	uint32_t name_hash;
};

struct job_cached_function_t
{
	job_function_t function;
	uint32_t name_hash;
};

struct job_queue_slot_t : public list_node_t<job_queue_slot_t>
{
	job_function_t function;
	job_event_t* depends;
	job_event_t* result;
	void* data;
};

struct job_context_t
{
	job_system_t* system;
	thread_t* worker_thread;
	volatile uint32_t command;

	allocator_t* incheap;
	job_queue_slot_t* curr_job;
};

struct job_event_t : public list_node_t<job_event_t>
{
	volatile int64_t num_left;
	volatile int64_t deps;
};

typedef std::unordered_map<uint32_t, job_bundle_t> bundle_map; // TODO: use own hash table?
typedef std::unordered_map<uint32_t, job_entry_t> function_map; // TODO: use own hash table?

struct job_system_t
{
	mutex_t* mutex;

	allocator_t* alloc;
	array_t<job_context_t> threads;
	linked_list_t<job_queue_slot_t> queue;
	linked_list_t<job_queue_slot_t> free_slots;
	objpool_t<job_cached_function_t, uint16_t> cached_functions;
	linked_list_t<job_event_t> free_events;

	bundle_map bundles;
	function_map functions;

	size_t max_job_argument_size;
	size_t job_argument_alignment;

	uint32_t main_thread_id;
	job_context_t main_thread_context;
};

static job_queue_slot_t* job_get_one(job_system_t* system)
{
	job_queue_slot_t* job = system->queue.front();
	while (job != nullptr)
	{
		if (job->depends == nullptr || job->depends->num_left == 0)
		{
			system->queue.remove(job);
			break;
		}
		else
		{
			job = system->queue.next(job);
		}
	}
	return job;
}

static void job_run_one(job_system_t* system, job_context_t* context)
{
	// TODO: a lot of locking going on :/ Rewrite most of it with atomic queues
	mutex_lock(system->mutex);
	if (system->queue.empty())
	{
		mutex_unlock(system->mutex);
		thread_yield(); // TODO: sleep on condition/semaphore?
		mutex_lock(system->mutex);
	}

	job_queue_slot_t* job = job_get_one(system);
	if (job)
	{
		mutex_unlock(system->mutex);

		context->curr_job = job;
		job->function(context, &job->data);
		allocator_incheap_reset(context->incheap);
		context->curr_job = nullptr;

		if (job->result)
			InterlockedDecrement64(&job->result->num_left);

		mutex_lock(system->mutex);

		if (job->depends)
		{
			uint64_t num_left = InterlockedDecrement64(&job->depends->deps);
			if (num_left == 0)
				system->free_events.push_back(job->depends);
		}

		system->free_slots.push_back(job);
	}
	mutex_unlock(system->mutex);
}

static void job_thread_main(void* ptr)
{
	job_context_t* context = reinterpret_cast<job_context_t*>(ptr);
	job_system_t* system = context->system;

	while(context->command != JOB_COMMAND_EXIT)
	{
		job_run_one(system, context);
	}
	thread_exit();
}

job_system_t* job_system_create(const job_system_create_params_t* params)
{
	job_system_t* system = ALLOCATOR_NEW(params->alloc, job_system_t);

	system->mutex = mutex_create();

	system->alloc = params->alloc;

	system->threads.create(system->alloc, params->num_threads);
	system->cached_functions.create(system->alloc, params->max_cached_functions);

	system->threads.set_length(system->threads.capacity());
	for(size_t i = 0; i < system->threads.length(); ++i)
	{
		system->threads[i].system = system;
		system->threads[i].incheap = allocator_incheap_create(system->alloc, params->worker_thread_temp_size);
		system->threads[i].worker_thread = thread_create("job_thread", job_thread_main, &system->threads[i]);
	}

	system->max_job_argument_size = params->max_job_argument_size;
	system->job_argument_alignment = params->job_argument_alignment;

	system->main_thread_id = thread_get_current_id(); // Assume creation thread is main thread
	memset(&system->main_thread_context, 0, sizeof(system->main_thread_context));
	system->main_thread_context.system = system;
	system->main_thread_context.incheap = allocator_incheap_create(system->alloc, params->worker_thread_temp_size);

	return system;
}

void job_system_destroy(job_system_t* system)
{
	for(size_t i = 0; i < system->threads.length(); ++i)
	{
		system->threads[i].command = JOB_COMMAND_EXIT;
	}

	for(size_t i = 0; i < system->threads.length(); ++i)
	{
		thread_join(system->threads[i].worker_thread);
		thread_destroy(system->threads[i].worker_thread);
		allocator_incheap_destroy(system->threads[i].incheap);
	}

	while (job_queue_slot_t* job = system->queue.pop_front())
	{
		system->free_events.push_back(job->result);
		system->free_slots.push_back(job);
	}
	while (job_queue_slot_t* job = system->free_slots.pop_front())
	{
		ALLOCATOR_FREE(system->alloc, job);
	}
	while (job_event_t* event = system->free_events.pop_front())
	{
		ALLOCATOR_FREE(system->alloc, event);
	}

	for(auto i : system->bundles)
	{
#if defined(FAMILY_WINDOWS)
		FreeLibrary(i.second.handle);
		BOOL delete_res = DeleteFile(i.second.file_name);
		ASSERT(delete_res != 0);
#elif defined(FAMILY_UNIX)
		dlclose(i.second.handle);
#endif //#if defined(FAMILY_*)
	}

	mutex_destroy(system->mutex);

	ALLOCATOR_DELETE(system->alloc, job_system_t, system);
}

job_system_result_t job_system_load_bundle(job_system_t* system, const char* bundle_name)
{
#if defined(FAMILY_WINDOWS)
	HMODULE handle = NULL;
#elif defined(FAMILY_UNIX)
	void* handle = NULL;
#endif //#if defined(FAMILY_*)
	uint32_t bundle_name_hash = hash_string(bundle_name);

	bundle_map::iterator bundle_iter = system->bundles.find(bundle_name_hash);

	if (bundle_iter != system->bundles.end())
	{
		function_map::iterator function_iter = system->functions.begin();
		while (function_iter != system->functions.end())
		{
			if (function_iter->second.bundle_name_hash == bundle_name_hash)
				system->functions.erase(function_iter);
			else
				++function_iter;
		}

		job_bundle_t* bundle = &bundle_iter->second;
#if defined(FAMILY_WINDOWS)
		FreeLibrary(bundle->handle);
		BOOL delete_res = DeleteFile(bundle->file_name);
		ASSERT(delete_res != 0);
#elif defined(FAMILY_UNIX)
			dlclose(system->bundles[i].handle);
#endif //#if defined(FAMILY_*)

		system->bundles.erase(bundle_iter);
	}

	job_bundle_t bundle;
#if defined(FAMILY_WINDOWS)
	const char* base_path = "local/build/winx64"; // TODO: don't hardcode this
	UINT unique = GetTempFileName(base_path, bundle_name, 0, bundle.file_name);
	ASSERT(unique != 0);

	char src_path[MAX_PATH];
	snprintf(src_path, ARRAY_LENGTH(src_path), "%s/%s", base_path, bundle_name);
	BOOL copy_res = CopyFile(src_path, bundle.file_name, FALSE);
	if (copy_res == 0)
	{
		// Kept around for debugging problems reloading
		DWORD err = GetLastError();
		LPTSTR lpMsgBuf;
		DWORD bufLen = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			err,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf,
			0, NULL );
		if (bufLen)
		{
			OutputDebugString(lpMsgBuf);
			LocalFree(lpMsgBuf);
		}
	
		return JOB_SYSTEM_COULD_NOT_LOAD_BUNDLE;
	}

	handle = LoadLibrary(bundle.file_name);
#elif defined(FAMILY_UNIX)
	handle = dlopen(name, RTLD_LAZY | RTLD_LOCAL);
#endif //#if defined(FAMILY_*)
	if(handle)
	{
#if defined(FAMILY_WINDOWS)
		const job_ptr_descriptor_t* table = (const job_ptr_descriptor_t*)GetProcAddress(handle, "job_table");
#elif defined(FAMILY_UNIX)
		const job_ptr_descriptor_t* table = (const job_ptr_descriptor_t*)dlsym(handle, "job_table");
#endif //#if defined(FAMILY_*)

		if(table)
		{
			bundle.handle = handle;
			bundle.name_hash = bundle_name_hash;
			while(table->function)
			{
				job_entry_t entry =
				{
					table->function,
					hash_string(table->name),
					bundle.name_hash
				};

				function_map::iterator function_iter = system->functions.find(entry.name_hash);
				ASSERT(function_iter == system->functions.end());

				system->functions[entry.name_hash] = entry;
				++table;
			}
			system->bundles[bundle_name_hash] = bundle;
			return JOB_SYSTEM_OK;
		}
#if defined(FAMILY_WINDOWS)
		FreeLibrary(handle);
#elif defined(FAMILY_UNIX)
		dlclose(handle);
#endif //#if defined(FAMILY_*)
		return JOB_SYSTEM_COULD_NOT_FIND_JOB_TABLE;
	}
	return JOB_SYSTEM_COULD_NOT_LOAD_BUNDLE;
}

job_system_result_t job_system_cache_function(job_system_t* system, uint32_t function_name_hash, job_cached_function_t** out_cached_function)
{
	function_map::iterator iter = system->functions.find(function_name_hash);
	if (iter == system->functions.end())
		return JOB_SYSTEM_FUNCTION_NOT_FOUND;

	if (system->cached_functions.full())
		return JOB_SYSTEM_TOO_MANY_CACHED_FUNCTIONS;

	job_cached_function_t* cached_function = system->cached_functions.alloc();
	cached_function->function = iter->second.function;
	cached_function->name_hash = function_name_hash;

	*out_cached_function = cached_function;
	return JOB_SYSTEM_OK;
}

job_system_result_t job_system_release_cached_function(job_system_t* system, job_cached_function_t* cached_function)
{
	system->cached_functions.free(cached_function);
	return JOB_SYSTEM_OK;
}

job_system_result_t job_system_kick(job_system_t* system, job_cached_function_t* cached_function, size_t num_jobs, void** args, size_t arg_size, job_event_t* depends, job_event_t** out_event)
{
	return job_system_kick_ptr(system, cached_function->function, num_jobs, args, arg_size, depends, out_event);
}

static job_system_result_t job_system_enqueue_jobs(job_system_t* system, job_function_t function, size_t num_jobs, void** args, size_t arg_size, job_event_t* depends, job_event_t** out_event)
{
	if (arg_size > system->max_job_argument_size)
		return JOB_SYSTEM_ARGUMENT_TOO_BIG;

	job_event_t* result_event = nullptr;
	if (out_event != nullptr)
	{
		if ((*out_event) == nullptr)
		{
			// We want an event, but has not yet allocated one. Do so now
			result_event = system->free_events.pop_front();
			if (result_event == nullptr)
			{
				result_event = ALLOCATOR_ALLOC_TYPE(system->alloc, job_event_t);
				memset(result_event, 0, sizeof(*result_event));
			}
			*out_event = result_event;
		}
		else
			result_event = *out_event;
	}

	if (result_event)
	{
		InterlockedAdd64(&result_event->num_left, num_jobs);
		InterlockedAdd64(&result_event->deps, 1);
	}
	if (depends)
	{
		InterlockedAdd64(&depends->deps, num_jobs);
	}

	// assumes the lock has been taken
	for (size_t i = 0; i < num_jobs; ++i)
	{
		job_queue_slot_t* slot = system->free_slots.pop_front();
		if (slot == nullptr)
		{
			slot = ALLOCATOR_ALLOC_TYPE(system->alloc, job_queue_slot_t);
			memset(slot, 0, sizeof(*slot));
			slot->data = ALLOCATOR_ALLOC(system->alloc, system->max_job_argument_size, system->job_argument_alignment); // TODO: merge allocs?
		}

		slot->function = function;
		slot->depends = depends;
		slot->result = result_event;

		if(arg_size > 0)
			memcpy(slot->data, args[i], arg_size);

		system->queue.push_back(slot);
	}

	return JOB_SYSTEM_OK;
}

job_system_result_t job_system_kick_ptr(job_system_t* system, job_function_t function, size_t num_jobs, void** args, size_t arg_size, job_event_t* depends, job_event_t** out_event)
{
	mutex_lock(system->mutex);

	job_system_result_t res = job_system_enqueue_jobs(system, function, num_jobs, args, arg_size, depends, out_event);

	mutex_unlock(system->mutex);

	return res;
}

job_system_result_t job_system_wait_release_event(job_system_t* system, job_event_t* event)
{
	ASSERT(thread_get_current_id() == system->main_thread_id);

	while (event->num_left != 0)
	{
		job_run_one(system, &system->main_thread_context);
	}

	mutex_lock(system->mutex);
	uint64_t num_left = InterlockedDecrement64(&event->deps);
	if (num_left == 0)
		system->free_events.push_back(event);

	mutex_unlock(system->mutex);

	return JOB_SYSTEM_OK;
}

job_system_result_t job_context_get_allocator(job_context_t* context, allocator_t** out_allocator)
{
	*out_allocator = context->incheap;
	return JOB_SYSTEM_OK;
}

job_system_result_t job_context_kick(job_context_t* context, job_cached_function_t* cached_function, size_t num_jobs, void** args, size_t arg_size)
{
	return job_context_kick_ptr(context, cached_function->function, num_jobs, args, arg_size);
}

job_system_result_t job_context_call(job_context_t* context, job_cached_function_t* cached_function, size_t num_jobs, void** args)
{
	for (size_t i = 0; i < num_jobs; ++i)
	{
		void* mark = allocator_incheap_curr(context->incheap);
		cached_function->function(context, args[i]);
		allocator_incheap_reset(context->incheap, mark);
	}
	return JOB_SYSTEM_OK;
}

job_system_result_t job_context_kick_ptr(job_context_t* context, job_function_t function, size_t num_jobs, void** args, size_t arg_size)
{
	mutex_lock(context->system->mutex);

	job_event_t** event = context->curr_job->result != nullptr ? &context->curr_job->result : nullptr;
	job_system_result_t res = job_system_enqueue_jobs(context->system, function, num_jobs, args, arg_size, context->curr_job->depends, event);

	mutex_unlock(context->system->mutex);

	return res;
}
