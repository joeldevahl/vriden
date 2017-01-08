#include "assert.h"
#include "hash.h"

#include "memory.h"
#include "allocator.h"

#include "array.h"
#include "circular_queue.h"
#include "objpool.h"

#include "thread.h"
#include "mutex.h"
#include "condition.h"
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

struct job_queue_slot_t
{
	job_function_t function;
	void* padding; // TODO: figure this out in a good way, 
	uint8_t ALIGN(16) data[1008];
};

struct job_thread_t
{
	job_system_t* system;
	thread_t* worker_thread;
	volatile uint32_t command;
};

struct job_event_t
{
	uint32_t num_kicked;
	volatile uint32_t num_finished;
	condition_t* cond;
};

typedef std::unordered_map<uint32_t, job_bundle_t> bundle_map; // TODO: use own hash table?
typedef std::unordered_map<uint32_t, job_entry_t> function_map; // TODO: use own hash table?

struct job_system_t
{
	mutex_t* mutex;
	condition_t* cond;

	allocator_t* alloc;
	array_t<job_thread_t> threads;
	circular_queue_t<job_queue_slot_t> queue;
	objpool_t<job_cached_function_t, uint16_t> cached_functions;

	bundle_map bundles;
	function_map functions;
};

static void job_thread_main(void* ptr)
{
	job_thread_t* thread = reinterpret_cast<job_thread_t*>(ptr);
	job_system_t* system = thread->system;
	while(thread->command != JOB_COMMAND_EXIT)
	{
		mutex_lock(system->mutex);

		if(system->queue.empty())
		{
			condition_wait(system->cond, system->mutex);
		}

		if(system->queue.any())
		{
			job_queue_slot_t job = system->queue.get();

			mutex_unlock(system->mutex);
			condition_signal(system->cond);

			job.function(&job.data);
		}
		else
		{
			mutex_unlock(system->mutex);
		}
	}
	thread_exit();
}

job_system_t* job_system_create(const job_system_create_params_t* params)
{
	job_system_t* system = ALLOCATOR_NEW(params->alloc, job_system_t);

	system->mutex = mutex_create();
	system->cond = condition_create();

	system->alloc = params->alloc;

	system->threads.create(system->alloc, params->num_threads);
	system->queue.create(system->alloc, params->num_queue_slots);
	system->cached_functions.create(system->alloc, params->max_cached_functions);

	system->threads.set_length(system->threads.capacity());
	for(size_t i = 0; i < system->threads.length(); ++i)
	{
		system->threads[i].system = system;
		system->threads[i].worker_thread = thread_create("job_thread", job_thread_main, &system->threads[i]);
	}

	return system;
}

void job_system_destroy(job_system_t* system)
{
	for(size_t i = 0; i < system->threads.length(); ++i)
	{
		system->threads[i].command = JOB_COMMAND_EXIT;
	}

	condition_broadcast(system->cond);

	for(size_t i = 0; i < system->threads.length(); ++i)
	{
		thread_join(system->threads[i].worker_thread);
		thread_destroy(system->threads[i].worker_thread);
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

	condition_destroy(system->cond);
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

job_system_result_t job_system_kick(job_system_t* system, job_cached_function_t* cached_function, void* arg, size_t arg_size)
{
	return job_system_kick_ptr(system, cached_function->function, arg, arg_size);
}

job_system_result_t job_system_kick_ptr(job_system_t* system, void (*function)(void*), void* arg, size_t arg_size)
{
	mutex_lock(system->mutex);

	if(system->queue.full())
	{
		condition_wait(system->cond, system->mutex);
	}
	
	job_queue_slot_t slot;
	ASSERT(arg_size < sizeof(slot.data), "Too much data in argument");
	slot.function = function;
	memcpy(&slot.data, arg, arg_size);
	system->queue.put(slot);

	mutex_unlock(system->mutex);
	condition_signal(system->cond);

	return JOB_SYSTEM_OK;
}
