#include <application/application.h>
#include <foundation/allocator.h>

int application_main(application_t* application);

int main(int argc, const char **argv)
{
	application_create_params_t params;
	application_t* application;

	params.allocator = &allocator_malloc;
	params.argc = argc;
	params.argv = argv;
	application = application_create(&params);

	int val = application_main(application);

	application_destroy(application);

	return val;
}
