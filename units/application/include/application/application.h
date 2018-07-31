#pragma once

struct application_create_params_t
{
	struct allocator_t* allocator;
	int argc;
	const char** argv;
};

struct application_t;

application_t* application_create(application_create_params_t* params);
void application_destroy(application_t* app);

void application_update(application_t* app);

int application_get_argc(application_t* app);
const char** application_get_argv(application_t* app);
bool application_is_running(application_t* app);
