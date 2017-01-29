#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>

#include "allocator.h"
#include "application.h"

struct application_t
{
	allocator_t* allocator;
	int argc;
	char** argv;

	bool running;
};

application_t* application_create(application_create_params_t* params)
{
	application_t* app = ALLOCATOR_ALLOC_TYPE(params->allocator, application_t);
	app->allocator = params->allocator;
	app->argc = params->argc;
	app->argv = params->argv;
	app->running = true;

	return app;
}

void application_destroy(application_t* app)
{
	ALLOCATOR_FREE(app->allocator, app);
}

void application_update(application_t* app)
{
	MSG msg;
	while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		if(msg.message == WM_QUIT)
			app->running = false;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

int application_get_argc(application_t* app)
{
	return app->argc;
}

char** application_get_argv(application_t* app)
{
	return app->argv;
}

bool application_is_running(application_t* app)
{
	return app->running;
}