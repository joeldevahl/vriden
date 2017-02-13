#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>

#include <foundation/assert.h>
#include <foundation/allocator.h>
#include <application/application.h>

int application_main(application_t* app);

int APIENTRY WinMain(HINSTANCE _In_ hInstance, HINSTANCE _In_opt_ hPrevInstance, LPSTR _In_ lpCmdLine, int _In_ nCmdShow)
{
	(void)hInstance;
	(void)hPrevInstance;
	(void)nCmdShow;

	application_create_params_t params;
	application_t* app;

	const size_t MAX_ARGS = 32;
	static char* argv[MAX_ARGS];
	int argc = 1;
	argv[0] = "appname.exe"; // TODO: get from hInstance?

	char* iter = lpCmdLine;
	while(*iter)
	{
		ASSERT(argc < MAX_ARGS);
		argv[argc++] = iter++;
		while(*iter != '\0' && *iter != ' ')
			++iter;

		if(*iter == ' ')
		{
			*iter = '\0';
			++iter;
		}
	}

	params.allocator = &allocator_malloc;
	params.argc = argc;
	params.argv = argv;
	app = application_create(&params);

	int val = application_main(app);

	application_destroy(app);

	return val;
}
