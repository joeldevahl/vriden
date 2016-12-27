#include "../job_system.h"

void dummy_job(void* data)
{
	volatile uint32_t* job_done = *(volatile uint32_t**)data;
	*job_done = 1u;
}

extern "C" __declspec(dllexport) const job_ptr_descriptor_t job_table[] = {
	{ dummy_job, "dummy_job" },
	{ 0, 0 },
};