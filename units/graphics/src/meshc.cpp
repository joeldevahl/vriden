#include <assert.h>
#include <hash.h>

#include <allocator.h>
#include <file.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <dl/dl.h>
#include <dl/dl_util.h>

#include <getopt/getopt.h>

#include <units/graphics/types/vertex_layout.h>
#include <units/graphics/types/mesh.h>
#include <units/graphics/types/mesh_intermediate.h>

static const unsigned char vertex_layout_typelib[] =
{
#include <units/graphics/types/vertex_layout.tlb.hex>
};

static const unsigned char mesh_typelib[] =
{
#include <units/graphics/types/mesh.tlb.hex>
};

static const unsigned char mesh_intermediate_typelib[] =
{
#include <units/graphics/types/mesh_intermediate.tlb.hex>
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
	dl_context_load_type_library(dl_ctx, vertex_layout_typelib, ARRAY_LENGTH(vertex_layout_typelib));
	dl_context_load_type_library(dl_ctx, mesh_intermediate_typelib, ARRAY_LENGTH(mesh_intermediate_typelib));
	dl_context_load_type_library(dl_ctx, mesh_typelib, ARRAY_LENGTH(mesh_typelib));

	mesh_intermediate_t* mesh_intermediate = nullptr;
	err = dl_util_load_from_file(
			dl_ctx,
			mesh_intermediate_t::TYPE_ID,
			infilename,
			DL_UTIL_FILE_TYPE_AUTO,
			(void**)&mesh_intermediate,
			nullptr);
	if(err != DL_ERROR_OK)
		ERROR_AND_FAIL("failed to load intermediate mesh from file \"%s\"", infilename);

	uint32_t vertex_layout_hash = hash_string(mesh_intermediate->vertex_stream.layout);
	vertex_layout_t* vertex_layout = nullptr;
	err = dl_util_load_from_file(
			dl_ctx,
			vertex_layout_t::TYPE_ID,
			mesh_intermediate->vertex_stream.layout,
			DL_UTIL_FILE_TYPE_AUTO,
			(void**)&vertex_layout,
			nullptr);
	if(err != DL_ERROR_OK)
		ERROR_AND_FAIL("failed to load vertex layout from file \"%s\"", mesh_intermediate->vertex_stream.layout);

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(
			mesh_intermediate->source,
			aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
	if(scene == NULL)
		ERROR_AND_FAIL("failed to load source mesh from file \"%s\"", mesh_intermediate->source);

	size_t num_vertices = 0;
	size_t num_faces = 0;

	for(size_t im = 0; im < scene->mNumMeshes; ++im)
	{
		const aiMesh* mesh = scene->mMeshes[0];
		ASSERT(mesh->HasPositions(), "Mesh must have positions");
		ASSERT(mesh->HasFaces(), "Mesh must have faces to be able to calculate indices");

		num_vertices += mesh->mNumVertices;
		num_faces += mesh->mNumFaces;
	}

	const uint8_t index_element_type = mesh_intermediate->index_stream.data_type; // TODO: hax
	const size_t index_element_size = sizeof(uint16_t); // TODO: hax
	const size_t num_indices = num_faces * 3;
	const size_t vertex_data_size = num_vertices*vertex_layout->stride;
	const size_t index_data_size = num_indices*index_element_size;

	// Dummy data to be used
	const float one[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	mesh_data_t mesh_data;

	mesh_data.vertex_data.layout_hash = vertex_layout_hash;
	mesh_data.vertex_data.data.data = (uint8_t*)ALLOCATOR_ALLOC(&allocator_malloc, vertex_data_size, 16);
	mesh_data.vertex_data.data.count = vertex_data_size;

	mesh_data.index_data.data_type = index_element_type;
	mesh_data.index_data.data.data = (uint8_t*)ALLOCATOR_ALLOC(&allocator_malloc, index_data_size, 16);
	mesh_data.index_data.data.count = index_data_size;


	uint8_t* vtx_ptr = &mesh_data.vertex_data.data[0];
	uint8_t* idx_ptr = &mesh_data.index_data.data[0];
	for(size_t im = 0; im < scene->mNumMeshes; ++im)
	{
		const aiMesh* mesh = scene->mMeshes[0];
		ASSERT(mesh->HasPositions(), "Mesh must have positions");
		const aiVector3D* positions = mesh->mVertices;
		ASSERT(mesh->HasFaces(), "Mesh must have faces to be able to calculate indices");
		const aiFace* faces = mesh->mFaces;

		for(size_t i = 0; i < mesh->mNumVertices; ++i)
		{
			for(size_t j = 0; j < vertex_layout->elements.count; ++j)
			{
				const float* src = NULL;
				switch(vertex_layout->elements[j].element_type)
				{
					case ELEMENT_TYPE_POSITION:
						src = reinterpret_cast<const float*>(positions + i);
						break;
					case ELEMENT_TYPE_ONE:
						src = one;
						break;
					default:
						BREAKPOINT();
				}

				const size_t stride = vertex_layout->elements[j].count;
				for(size_t k = 0; k < stride; ++k)
				{
					float f = src[k];
					switch(vertex_layout->elements[j].data_type)
					{
						case 0: // TODO: hax
						{
							memcpy(vtx_ptr, &f, sizeof(float));
							vtx_ptr += sizeof(float);
							break;
						}
						default:
						{
							BREAKPOINT();
						}
					}
				}
			}
		}

		for(size_t i = 0; i < mesh->mNumFaces; ++i)
		{
			ASSERT(faces[i].mNumIndices == 3, "Only triangles supported for now");
			for(size_t j = 0; j < 3; ++j)
			{
				switch(index_element_type)
				{
					case 2: // TODO: hax
					{
						uint16_t idx = faces[i].mIndices[j];
						memcpy(idx_ptr, &idx, sizeof(idx));
						idx_ptr += sizeof(idx);
						break;
					}

					default:
					{
						BREAKPOINT();
					}
				}
			}
		}
	}

	err = dl_util_store_to_file(
			dl_ctx,
			mesh_data_t::TYPE_ID,
			outfilename,
			DL_UTIL_FILE_TYPE_BINARY,
			dl_endian_host(),
			sizeof(void*),
			&mesh_data);
	if(err != DL_ERROR_OK)
		ERROR_AND_FAIL("failed to save compiled mesh to file \"%s\"", outfilename);

	return 0;
}
