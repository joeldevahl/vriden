#include <foundation/assert.h>
#include <foundation/hash.h>

#include <foundation/allocator.h>
#include <foundation/file.h>

#include <dl/dl.h>
#include <dl/dl_util.h>

#include <getopt/getopt.h>

#include <units/graphics/types/texture.h>

static const unsigned char texture_typelib[] =
{
#include <units/graphics/types/texture.tlb.hex>
};

#define ERROR_AND_QUIT(fmt, ...) { fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__); return 0x0; }
#define ERROR_AND_FAIL(fmt, ...) { fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__); return 1; }

void print_help(getopt_context_t* ctx)
{
	char buffer[2048];
	printf("usage: meshc [options] file\n\n");
	printf("%s", getopt_create_help_string(ctx, buffer, sizeof(buffer)));
}

int main(int argc, const char** argv)
{
	static const getopt_option_t option_list[] =
	{
		{ "help",   'h', GETOPT_OPTION_TYPE_NO_ARG,   0x0, 'h', "displays this message", 0x0 },
		{ "output", 'o', GETOPT_OPTION_TYPE_REQUIRED, 0x0, 'o', "output to file", "file" },
		GETOPT_OPTIONS_END
	};

	getopt_context_t go_ctx;
	getopt_create_context(&go_ctx, argc, argv, option_list);

	const char* infilename = nullptr;
	const char* outfilename = nullptr;

	int opt;
	while( (opt = getopt_next( &go_ctx ) ) != -1 )
	{
		switch(opt)
		{
			case 'h': print_help(&go_ctx); return 0;
			case 'o':
				if(outfilename != nullptr && outfilename[0] != '\0')
					ERROR_AND_FAIL("output file already set to: \"%s\", trying to set it to \"%s\"", outfilename, go_ctx.current_opt_arg);

				outfilename = go_ctx.current_opt_arg;
				break;
			case '+':
				if(infilename != nullptr && infilename[0] != '\0')
					ERROR_AND_FAIL("input file already set to: \"%s\", trying to set it to \"%s\"", infilename, go_ctx.current_opt_arg);

				infilename = go_ctx.current_opt_arg;
				break;
			case 0: break; // ignore, flag was set!
		}
	}

	if(infilename == nullptr)
		ERROR_AND_FAIL("input file not set");
	if(outfilename == nullptr)
		ERROR_AND_FAIL("output file not set");

	dl_ctx_t dl_ctx;
	dl_create_params_t dl_create_params;
	DL_CREATE_PARAMS_SET_DEFAULT(dl_create_params);
	dl_error_t err = dl_context_create(&dl_ctx, &dl_create_params);
	dl_context_load_type_library(dl_ctx, texture_typelib, ARRAY_LENGTH(texture_typelib));

	texture_data_t texture_data;

	err = dl_util_store_to_file(
			dl_ctx,
			texture_data_t::TYPE_ID,
			outfilename,
			DL_UTIL_FILE_TYPE_BINARY,
			dl_endian_host(),
			sizeof(void*),
			&texture_data);
	if(err != DL_ERROR_OK)
		ERROR_AND_FAIL("failed to save compiled texture to file \"%s\"", outfilename);

	return 0;
}
