#include "../job_system.h"

struct dummy_job_params_t
{
	volatile uint32_t* job_done_ptr;
};

void dummy_job(job_context_t* /*context*/, void* data)
{
	dummy_job_params_t* params = (dummy_job_params_t*)data;
	*(params->job_done_ptr) = 1u;
}

extern "C" __declspec(dllexport) const job_ptr_descriptor_t job_table[] = {
	{ dummy_job, "dummy_job" },
	{ 0, 0 },
};