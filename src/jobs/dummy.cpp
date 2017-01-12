#include "../job_system.h"

void dummy_job_spawned(job_context_t* /*context*/, void* /*data*/)
{
}

void dummy_job(job_context_t* context, void* /*data*/)
{
	job_context_kick_ptr(context, dummy_job_spawned, 1024, nullptr, 0);
}

extern "C" __declspec(dllexport) const job_ptr_descriptor_t job_table[] = {
	{ dummy_job_spawned, "dummy_job_spawned" },
	{ dummy_job, "dummy_job" },
	{ 0, 0 },
};