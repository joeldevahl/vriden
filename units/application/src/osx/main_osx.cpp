#include <application.h>
#include <allocator.h>

int atec_main(application_t* application);

int main(int argc, char **argv)
{
	application_create_params_t params;
	application_t* application;

	params.allocator = &allocator_malloc;
	params.argc = argc;
	params.argv = argv;
	application = application_create(&params);

	int val = atec_main(application);

	application_destroy(application);

	return val;
}
