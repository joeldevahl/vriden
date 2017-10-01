#include <foundation/assert.h>
#include <foundation/hash.h>

#include <foundation/allocator.h>
#include <foundation/file.h>

#include <dl/dl.h>
#include <dl/dl_util.h>

#include <getopt/getopt.h>

#include <units/graphics/types/material.h>
#include <units/graphics/types/material_intermediate.h>

static const unsigned char material_typelib[] =
{
#include <units/graphics/types/material.tlb.hex>
};

static const unsigned char material_intermediate_typelib[] =
{
#include <units/graphics/types/material_intermediate.tlb.hex>
};

#define ERROR_AND_QUIT(fmt, ...) { fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__); return 0x0; }
#define ERROR_AND_FAIL(fmt, ...) { fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__); return 1; }

void print_help(getopt_context_t* ctx)
{
	char buffer[2048];
	printf("usage: materialc [options] file\n\n");
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

	const char* infilename = NULL;
	const char* outfilename = NULL;

	int opt;
	while( (opt = getopt_next( &go_ctx ) ) != -1 )
	{
		switch(opt)
		{
			case 'h': print_help(&go_ctx); return 0;
			case 'o':
				if(outfilename != NULL && outfilename[0] != '\0')
				{
					ERROR_AND_FAIL("output-file already set to: \"%s\", trying to set it to \"%s\"", outfilename, go_ctx.current_opt_arg);
				}

				outfilename = go_ctx.current_opt_arg;
				break;
			case '+':
				if(infilename != NULL && infilename[0] != '\0')
				{
					ERROR_AND_FAIL("input-file already set to: \"%s\", trying to set it to \"%s\"", infilename, go_ctx.current_opt_arg);
				}

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
	dl_context_load_type_library(dl_ctx, material_intermediate_typelib, ARRAY_LENGTH(material_intermediate_typelib));
	dl_context_load_type_library(dl_ctx, material_typelib, ARRAY_LENGTH(material_typelib));

	material_intermediate_t* material_intermediate = nullptr;
	err = dl_util_load_from_file(
			dl_ctx,
			material_intermediate_t::TYPE_ID,
			infilename,
			DL_UTIL_FILE_TYPE_AUTO,
			(void**)&material_intermediate,
			nullptr);
	if(err != DL_ERROR_OK)
		ERROR_AND_FAIL("failed to load intermediate material from file \"%s\"", infilename);

	// TODO: resolve at compile time since we know actual shader here?
	uint32_t num_properties = material_intermediate->properties.count;
	material_property_t* properties = (material_property_t*)alloca(num_properties * sizeof(material_property_t));

	for (size_t i = 0; i < num_properties; ++i)
	{
		properties[i].name_hash = hash_string(material_intermediate->properties[i].name);
		if (material_intermediate->properties[i].path != nullptr)
		{
			properties[i].value.data = nullptr;
			properties[i].value.count = 0;
		}
		else
		{
			properties[i].value.data = (uint8_t*)material_intermediate->properties[i].value.data;
			properties[i].value.count = material_intermediate->properties[i].value.count * sizeof(float);
		}
	}

	material_data_t material_data =
	{
		{ properties, num_properties },
		hash_string(material_intermediate->shader_name)
	};

	err = dl_util_store_to_file(
			dl_ctx,
			material_data_t::TYPE_ID,
			outfilename,
			DL_UTIL_FILE_TYPE_BINARY,
			dl_endian_host(),
			sizeof(void*),
			&material_data);
	if (err != DL_ERROR_OK)
		ERROR_AND_FAIL("failed to save compiled material to file \"%s\"", outfilename);

	return 0;
}
