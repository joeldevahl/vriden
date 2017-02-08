#include <foundation/job_system.h>

void dummy_job_spawned(job_context_t* /*context*/, void* /*data*/)
{
}

void dummy_job(job_context_t* context, void* /*data*/)
{
	job_context_kick_ptr(context, dummy_job_spawned, 128, nullptr, 0);
}

void dummy_job2(job_context_t* context, void* /*data*/)
{
	job_context_kick_ptr(context, dummy_job_spawned, 128, nullptr, 0);
}

extern "C" SHARED_LIBRARY_EXPORT const job_ptr_descriptor_t job_table[] = {
	{ dummy_job_spawned, "dummy_job_spawned" },
	{ dummy_job, "dummy_job" },
	{ dummy_job2, "dummy_job2" },
	{ 0, 0 },
};
