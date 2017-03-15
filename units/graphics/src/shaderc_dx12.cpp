#if defined(FAMILY_WINDOWS)

#include <foundation/defines.h>
#include <foundation/hash.h>

#include <d3dcompiler.h>
#include <d3d12.h>

#include <string>

#include <units/graphics/types/shader.h>
#include <units/graphics/types/shader_intermediate.h>

#define ERROR_AND_QUIT(fmt, ...) { fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__); return 0x0; }
#define ERROR_AND_FAIL(fmt, ...) { fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__); return 1; }

static int compile_program_dx12(const std::string& code, int stage, uint8_t** out_data, uint32_t* out_size)
{
	// DX11 shader, concatenate with header and compile using the d3d compiler
    DWORD flags = D3DCOMPILE_ENABLE_STRICTNESS;
    flags |= D3DCOMPILE_DEBUG;

    ID3DBlob* error = NULL;
    ID3DBlob* shader = NULL;
	const char* shader_model[] = {
		"vs_5_1",
		"hs_5_1",
		"ds_5_1",
		"gs_5_1",
		"ps_5_1",
	};

	//printf("compiling code:\n%s\n", code.c_str());

    HRESULT hr = D3DCompile2(code.c_str(),
							code.size(),
							nullptr, // file name
							nullptr, // defines
							NULL, // includes
							"main",
							shader_model[stage],
							flags,
							0, // flags2
							0, // secondary data flags
							NULL, // secondary data
							0, // secondary data size
							&shader,
							&error);
    if(FAILED(hr))
    {
        if(error != NULL)
		{
			printf("HLSL compiler failed with message:\n%s", (char*)error->GetBufferPointer());
			error->Release();
		}
		else
		{
			printf("No Error :(\n");
		}
        return hr;
    }

	*out_data = (uint8_t*)shader->GetBufferPointer();
	*out_size = (uint32_t)shader->GetBufferSize();
	//shader->Release(); We skip this since we know we are not a long running process

	return 0;
}

bool compile_shader_dx12(shader_intermediate_t* shader_intermediate, shader_data_dx12_t* shader_data)
{
	size_t num_properties = shader_intermediate->properties.count;
	shader_property_dx12_t* properties = (shader_property_dx12_t*)malloc(num_properties * sizeof(shader_property_dx12_t));
	shader_data->properties.data = properties;
	shader_data->properties.count = (uint32_t)num_properties;

	shader_constant_buffer_dx12_t* constant_buffers = shader_data->constant_buffers;
	std::string constant_buffer_code[SHADER_FREQUENCY_MAX] = {};

	constant_buffer_code[SHADER_FREQUENCY_PER_FRAME] = "cbuffer per_frame : register(b0)\n{\n";
	constant_buffer_code[SHADER_FREQUENCY_PER_VIEW] = "cbuffer per_view : register(b0)\n{\n";
	constant_buffer_code[SHADER_FREQUENCY_PER_PASS] = "cbuffer per_pass : register(b0)\n{\n";
	constant_buffer_code[SHADER_FREQUENCY_PER_MATERIAL] = "cbuffer per_material : register(b0)\n{\n";
	constant_buffer_code[SHADER_FREQUENCY_PER_INSTANCE] = "cbuffer per_instance : register(b0)\n{\n";

	char itoa_scratch[21];
	for (size_t i = 0; i < num_properties; ++i)
	{
		properties[i].name_hash = hash_string(shader_intermediate->properties[i].name);
		shader_frequency_t freq = properties[i].frequency = (shader_frequency_t)shader_intermediate->properties[i].frequency;
		switch (shader_intermediate->properties[i].data.type)
		{
			case shader_intermediate_property_data_t_type_floats:
			{
				const size_t num = shader_intermediate->properties[i].data.value.floats.count;
				properties[i].data.count = shader_intermediate->properties[i].data.value.floats.count * sizeof(float);
				properties[i].data.data = (uint8_t*)shader_intermediate->properties[i].data.value.floats.data;
				properties[i].pack_offset = constant_buffers[0].data.count;
				constant_buffers[freq].data.count += shader_intermediate->properties[i].data.value.floats.count * sizeof(float); // TODO: alignment
				constant_buffer_code[freq] += "\t float";
				constant_buffer_code[freq] += _itoa((int)num, itoa_scratch, 10);
				constant_buffer_code[freq] += " ";
				constant_buffer_code[freq] += shader_intermediate->properties[i].name;
				constant_buffer_code[freq] += ";\n";
				break;
			}
			case shader_intermediate_property_data_t_type_ints:
			{
				const size_t num = shader_intermediate->properties[i].data.value.ints.count;
				properties[i].data.count = shader_intermediate->properties[i].data.value.ints.count * sizeof(int32_t);
				properties[i].data.data = (uint8_t*)shader_intermediate->properties[i].data.value.ints.data;
				properties[i].pack_offset = constant_buffers[0].data.count;
				constant_buffers[freq].data.count += shader_intermediate->properties[i].data.value.ints.count * sizeof(int32_t); // TODO: alignment
				constant_buffer_code[freq] += "\t int";
				constant_buffer_code[freq] += _itoa((int)num, itoa_scratch, 10);
				constant_buffer_code[freq] += " ";
				constant_buffer_code[freq] += shader_intermediate->properties[i].name;
				constant_buffer_code[freq] += ";\n";
				break;
			}
		}
	}

	for (int i = 0; i < SHADER_FREQUENCY_MAX; ++i)
	{
		constant_buffer_code[i] += "};";

		if (constant_buffers[i].data.count > 0)
		{
			constant_buffers[i].data.data = (uint8_t*)malloc(constant_buffers[i].data.count);
			memset(constant_buffers[i].data.data, 0, constant_buffers[i].data.count);
		}
	}

	for (size_t i = 0; i < num_properties; ++i)
	{
		shader_frequency_t idx = properties[i].frequency;
		uint8_t* dst = constant_buffers[idx].data.data;
		switch (shader_intermediate->properties[i].data.type)
		{
			case shader_intermediate_property_data_t_type_floats:
			{
				memcpy(dst + properties[i].pack_offset, properties[i].data.data, properties[i].data.count);
				break;
			}
			case shader_intermediate_property_data_t_type_ints:
			{
				memcpy(dst + properties[i].pack_offset, properties[i].data.data, properties[i].data.count);
				break;
			}
		}
	}

	shader_variant_data_dx12_t* variants = (shader_variant_data_dx12_t*)calloc(1, sizeof(shader_variant_data_dx12_t));
	shader_data->variants.count = 1;
	shader_data->variants.data = variants;

	std::string common_code = shader_intermediate->common_code;
	for (int i = 0; i < SHADER_FREQUENCY_MAX; ++i)
	{
		common_code += "\n\n";
		common_code += constant_buffer_code[i];
		common_code += "\n\n";
	}

	for (int i = 0; i < SHADER_STAGE_MAX; ++i)
	{
		const char* raw_stage_code = NULL;
		switch (i)
		{
		case SHADER_STAGE_VERTEX:   raw_stage_code = shader_intermediate->vertex_program; break;
		case SHADER_STAGE_HULL:     raw_stage_code = shader_intermediate->hull_program; break;
		case SHADER_STAGE_DOMAIN:   raw_stage_code = shader_intermediate->domain_program; break;
		case SHADER_STAGE_GEOMETRY: raw_stage_code = shader_intermediate->geometry_program; break;
		case SHADER_STAGE_FRAGMENT: raw_stage_code = shader_intermediate->fragment_program; break;
		}

		if (raw_stage_code == NULL || strlen(raw_stage_code) <= 0)
			continue;

		std::string stage_code = common_code;
		stage_code += raw_stage_code;
		int res = compile_program_dx12(stage_code, i, &variants[0].stages[i].program.data, &variants[0].stages[i].program.count);
		if (res != 0)
			ERROR_AND_FAIL("Shader failed to compile, see message above");
	}

	return true;
}

#endif
