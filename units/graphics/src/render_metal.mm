#include "render_metal.h"
#include <foundation/allocator.h>
#include <foundation/hash.h>

#include <algorithm>

render_result_t render_metal_create(const render_create_info_t* create_info, render_metal_t** out_render)
{
	render_metal_t* render = ALLOCATOR_NEW(create_info->allocator, render_metal_t);
	render->allocator = create_info->allocator;
	render->backend = RENDER_BACKEND_METAL;

	render->views.create(render->allocator, 1);
	render->scripts.create(render->allocator, 1);
	render->textures.create(render->allocator, create_info->max_textures);
	render->shaders.create(render->allocator, create_info->max_shaders);
	render->materials.create(render->allocator, create_info->max_materials);
	render->meshes.create(render->allocator, create_info->max_meshes);
	render->instances.create(render->allocator, create_info->max_instances);

	render->window = (NSWindow*)create_info->window;
	render->view = [render->window contentView];
	render->layer = (CAMetalLayer*)[render->view layer];

	render->device = MTLCreateSystemDefaultDevice();
	render->layer.device = render->device;
	render->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	render->layer.allowsNextDrawableTimeout = NO;
	render->command_queue = [render->device newCommandQueue];

	*out_render = render;
	return RENDER_RESULT_OK;
}

void render_metal_destroy(render_metal_t* render)
{
	[render->command_queue release];
	[render->device release];
	ALLOCATOR_DELETE(render->allocator, render_metal_t, render);
}

void render_metal_wait_idle(render_metal_t* /*render*/)
{
}

render_result_t render_metal_view_create(render_metal_t* render, const render_view_create_info_t* create_info, render_view_id_t* out_view_id)
{
	ASSERT(render->views.num_free() != 0);

	render_metal_view_t* view = render->views.alloc();
	view->data = create_info->initial_data;

	*out_view_id = render->views.pointer_to_handle(view);
	return RENDER_RESULT_OK;
}

void render_metal_view_destroy(render_metal_t* render, render_view_id_t view_id)
{
	//render_metal_view_t* view = render->views.handle_to_pointer(view_id);

	render->views.free_handle(view_id);
}

render_result_t render_metal_script_create(render_metal_t* render, const render_script_create_info_t* /*create_info*/, render_script_id_t* out_script_id)
{
	ASSERT(render->scripts.num_free() != 0);

	render_metal_script_t* script = render->scripts.alloc();

	*out_script_id = render->scripts.pointer_to_handle(script);
	return RENDER_RESULT_OK;
}

void render_metal_script_destroy(render_metal_t* render, render_script_id_t script_id)
{
	//render_metal_script_t* script = render->scripts.handle_to_pointer(script_id);
	render->scripts.free_handle(script_id);
}

render_result_t render_metal_texture_create(render_metal_t* render, const texture_data_t* texture_data, render_texture_id_t* out_texture_id)
{
	ASSERT(render->textures.num_free() != 0);

	render_metal_texture_t* texture = render->textures.alloc();
	texture->width = texture_data->width;
	texture->height = texture_data->height;

	*out_texture_id = render->textures.pointer_to_handle(texture);
	return RENDER_RESULT_OK;
}

void render_metal_texture_destroy(render_metal_t* render, render_texture_id_t texture_id)
{
	//render_metal_texture_t* texture = render->textures.handle_to_pointer(texture_id);

	render->textures.free_handle(texture_id);
}

render_result_t render_metal_shader_create(render_metal_t* render, const shader_data_t* shader_data, render_shader_id_t* out_shader_id)
{
	ASSERT(shader_data->data_metal != nullptr);
	ASSERT(render->shaders.num_free() != 0);

	//const shader_data_metal_t* data = shader_data->data_metal;

	render_metal_shader_t* shader = render->shaders.alloc();

	*out_shader_id = render->shaders.pointer_to_handle(shader);
	return RENDER_RESULT_OK;
}

void render_metal_shader_destroy(render_metal_t* render, render_shader_id_t shader_id)
{
	//render_metal_shader_t* shader = render->shaders.handle_to_pointer(shader_id);

	render->shaders.free_handle(shader_id);
}

render_result_t render_metal_material_create(render_metal_t* render, const material_data_t* material_data, render_material_id_t* out_material_id)
{
	ASSERT(render->materials.num_free() != 0);

	render_metal_material_t* material = render->materials.alloc();
	material->shader = render->shaders.handle_to_pointer(material_data->shader_name_hash);

	*out_material_id = render->materials.pointer_to_handle(material);
	return RENDER_RESULT_OK;
}

void render_metal_material_destroy(render_metal_t* render, render_material_id_t material_id)
{
	//render_metal_material_t* material = render->materials.handle_to_pointer(material_id);

	render->materials.free_handle(material_id);
}

render_result_t render_metal_mesh_create(render_metal_t* render, const mesh_data_t* /*mesh_data*/, render_mesh_id_t* out_mesh_id)
{
	ASSERT(render->meshes.num_free() != 0);

	render_metal_mesh_t* mesh = render->meshes.alloc();

	*out_mesh_id = render->meshes.pointer_to_handle(mesh);
	return RENDER_RESULT_OK;
}

void render_metal_mesh_destroy(render_metal_t* render, render_mesh_id_t mesh_id)
{
	//render_metal_mesh_t* mesh = render->meshes.handle_to_pointer(mesh_id);

	render->meshes.free_handle(mesh_id);
}

render_result_t render_metal_instance_create(render_metal_t* render, const render_instance_create_info_t* create_info, render_instance_id_t* out_instance_id)
{
	ASSERT(render->instances.num_free() != 0);

	render_metal_instance_t* instance = render->instances.alloc();
	instance->mesh = render->meshes.handle_to_pointer(create_info->mesh_id);
	instance->material = render->materials.handle_to_pointer(create_info->material_id);

	instance->data = create_info->initial_data;

	*out_instance_id = render->instances.pointer_to_handle(instance);
	return RENDER_RESULT_OK;
}

void render_metal_instance_destroy(render_metal_t* render, render_instance_id_t instance_id)
{
	//render_metal_instance_t* instance = render->instances.handle_to_pointer(instance_id);
	
	render->instances.free_handle(instance_id);
}

render_result_t render_metal_instance_set_data(render_metal_t* render, size_t num_instances, render_instance_id_t* instance_ids, render_instance_data_t* instance_data)
{
	// TODO: do this a more efficient way?
	render_metal_instance_t* instances = render->instances.base_ptr();
	render_instance_id_t* instance_indices = render->instances.index_ptr();
	for (size_t i = 0; i < num_instances; ++i)
	{
		render_metal_instance_t* instance = &instances[instance_indices[instance_ids[i]]]; // :E
		memcpy(&instance->data, &instance_data[i], sizeof(instance->data));
	}

	return RENDER_RESULT_OK;
}

void render_metal_kick_render(render_metal_t* render, render_view_id_t /*view_id*/, render_script_id_t /*script_id*/)
{
	@autoreleasepool
	{
		id<CAMetalDrawable> drawable = [render->layer nextDrawable];

		id<MTLCommandBuffer> command_buffer = [render->command_queue commandBuffer];

		MTLRenderPassDescriptor* render_pass = [MTLRenderPassDescriptor renderPassDescriptor];
		render_pass.colorAttachments[0].texture = drawable.texture;
		render_pass.colorAttachments[0].loadAction = MTLLoadActionClear;
		render_pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		render_pass.colorAttachments[0].clearColor = MTLClearColorMake(0.3f, 0.3f, 0.6f, 1.0f);

		id <MTLRenderCommandEncoder> command_encoder = [command_buffer renderCommandEncoderWithDescriptor: render_pass];
		[command_encoder endEncoding];

		[command_buffer presentDrawable:drawable];
		[command_buffer commit];
	}
}

void render_metal_kick_upload(render_metal_t* /*render*/)
{
}
