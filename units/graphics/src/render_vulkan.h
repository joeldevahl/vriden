#pragma once

#include <graphics/render.h>
#include "render_common.h"
#include <foundation/array.h>
#include <foundation/objpool.h>
#include <foundation/cobjpool.h>
#include <foundation/idpool.h>

// define these... vulkan.h pulls in windows.h if we want to use PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#define NOMINMAX

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

/* external types */
#include <units/graphics/types/mesh.h>
#include <units/graphics/types/shader.h>
#include <units/graphics/types/texture.h>
#include <units/graphics/types/material.h>

struct render_vulkan_view_t
{
	render_view_data_t data;
};

struct render_vulkan_target_t
{
	uint32_t name_hash;
};

struct render_vulkan_pass_t
{
	size_t num_color_attachments;
	bool has_depth_stencil_attachment;

	size_t num_commands;
	render_command_t* commands;
};

struct render_vulkan_script_t
{
	size_t num_targets;
	render_vulkan_target_t* targets;

	size_t num_passes;
	render_vulkan_pass_t* passes;
};

struct render_vulkan_texture_t
{
	uint16_t width;
	uint16_t height;
};

struct render_vulkan_shader_property_t
{
	uint32_t name_hash;
	shader_frequency_t frequency;
	uint32_t pack_offset;
	uint32_t pack_size;
};

struct render_vulkan_shader_stage_t
{
};

struct render_vulkan_shader_variant_t
{
	VkShaderModule shader_modules[SHADER_STAGE_MAX]; // TODO: only one module per variant, or even shader?
	VkPipeline pipeline;
};

struct render_vulkan_constant_buffer_pool_t
{
	size_t stride;
	uint8_t* default_data;
	//ID3D12Resource* pool_buffer;
	idpool_t<uint16_t> pool;
};

struct render_vulkan_shader_t
{
	render_vulkan_constant_buffer_pool_t constant_buffers[SHADER_FREQUENCY_MAX];

	size_t num_properties;
	render_vulkan_shader_property_t* properties;

	size_t num_variants;
	render_vulkan_shader_variant_t* variants;
};

typedef enum render_vulkan_property_type_t
{
	RENDER_PROPERTY_FLOAT_SCALAR = 0,
	RENDER_PROPERTY_INT_SCALAR,
	RENDER_PROPERTY_FLOAT_VEC4,
	RENDER_PROPERTY_FLOAT_MAT4,
	RENDER_PROPERTY_TEXTURE,
} render_vulkan_property_type_t;

struct render_vulkan_material_t
{
	render_vulkan_shader_t* shader;

	uint16_t constant_buffer_ids[SHADER_FREQUENCY_MAX];
	uint8_t* constant_buffers[SHADER_FREQUENCY_MAX]; // CPU side
};

struct render_vulkan_mesh_t
{
	size_t num_vertices;
	size_t num_indices;
};

struct render_vulkan_instance_t
{
	render_vulkan_mesh_t* mesh;
	render_vulkan_material_t* material;
	render_instance_data_t data;
};

#define RENDER_VULKAN_MULTI_BUFFERING 3

struct render_vulkan_t : public render_t
{
	allocator_t* allocator;

	objpool_t<render_vulkan_view_t,      render_view_id_t>     views;
	objpool_t<render_vulkan_script_t,    render_script_id_t>   scripts;
	objpool_t<render_vulkan_texture_t,   render_texture_id_t>  textures;
	objpool_t<render_vulkan_shader_t,    render_shader_id_t>   shaders;
	objpool_t<render_vulkan_material_t,  render_material_id_t> materials;
	objpool_t<render_vulkan_mesh_t,      render_mesh_id_t>     meshes;

	cobjpool_t<render_vulkan_instance_t, render_instance_id_t> instances;

	VkInstance instance;
	VkDebugReportCallbackEXT debug_callback;
	VkSurfaceKHR main_surface;
	VkExtent2D extent;

	VkPhysicalDevice physical_device;
	int graphics_queue_index;
	int present_queue_index;

	VkDevice device;
	VkQueue graphics_queue;
	VkQueue present_queue;

	VkSwapchainKHR swap_chain;
	VkFormat swap_chain_image_format;
	array_t<VkImage> swap_chain_images;
	array_t<VkImageView> swap_chain_image_views;
	VkSemaphore backbuffer_semaphore;
	VkSemaphore present_semaphore;

	array_t<VkFramebuffer> swap_chain_framebuffers;

	array_t<VkCommandPool> command_pools;
	array_t<VkFence> frame_fences;

	VkRenderPass temp_render_pass;
	VkDescriptorSetLayout layout_per_frame;
	VkPipelineLayout temp_pipeline_layout;
	VkPipelineCache pipeline_cache;
};
