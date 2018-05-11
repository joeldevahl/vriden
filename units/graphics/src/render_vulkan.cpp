#include "render_vulkan.h"
#include <foundation/allocator.h>
#include <foundation/assert.h>
#include <foundation/hash.h>

#include <algorithm>

template<class T>
static void render_vulkan_filter_layer_strings(const scoped_array_t<T>& src, scoped_array_t<const char*>& dst, const char** wanted, uint32_t wanted_count)
{
	for (uint32_t is = 0; is < src.length(); ++is)
	{
		for (uint32_t iw = 0; iw < wanted_count; ++iw)
		{
			const char* name = src[is].layerName;
			if(strcmp(name, wanted[iw]) == 0)
			{
				dst.grow_and_append(name);
				break;
			}
		}
	}
}

static void render_vulkan_create_instance(render_vulkan_t* render)
{
	uint32_t num_available_instance_layers = 0;
	VkResult res = vkEnumerateInstanceLayerProperties(&num_available_instance_layers, nullptr);
	ASSERT(res == VK_SUCCESS, "Failed to get instance layer count");
	scoped_array_t<VkLayerProperties> instance_layer_props(render->allocator, num_available_instance_layers);
	res = vkEnumerateInstanceLayerProperties(&num_available_instance_layers, instance_layer_props.begin());
	ASSERT(res == VK_SUCCESS, "Failed to get instance layer properties");

	const char* wanted_layers[] = {
		"VK_LAYER_LUNARG_standard_validation",
	};
	scoped_array_t<const char*> enabled_layers(render->allocator);
	render_vulkan_filter_layer_strings(instance_layer_props, enabled_layers, wanted_layers, ARRAY_LENGTH(wanted_layers));

	// TODO: extension selection
	const char* wanted_extensions[] = {
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
		VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(FAMILY_WINDOWS)
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#else
#		error not implemented for this platform
#endif
	};

	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "vriden";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "vriden";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.enabledExtensionCount = ARRAY_LENGTH(wanted_extensions);
	create_info.ppEnabledExtensionNames = wanted_extensions;
	create_info.enabledLayerCount = (uint32_t)enabled_layers.length();
	create_info.ppEnabledLayerNames = enabled_layers.begin();

	VkResult vkres = vkCreateInstance(&create_info, nullptr, &render->instance);
	ASSERT(vkres == VK_SUCCESS);
}

static void render_vulkan_destroy_instance(render_vulkan_t* render)
{
	vkDestroyInstance(render->instance, nullptr);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL render_vulkan_debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData)
{
	BREAKPOINT();
	return VK_FALSE;
}

static void render_vulkan_create_debug_report(render_vulkan_t* render)
{
	VkDebugReportCallbackCreateInfoEXT create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	create_info.pfnCallback = &render_vulkan_debug_callback;
	create_info.pUserData = render;

	auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(render->instance, "vkCreateDebugReportCallbackEXT");
	vkCreateDebugReportCallbackEXT(render->instance, &create_info, nullptr, &render->debug_callback);
}

static void render_vulkan_destroy_debug_report(render_vulkan_t* render)
{
	auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(render->instance, "vkDestroyDebugReportCallbackEXT");
	vkDestroyDebugReportCallbackEXT(render->instance, render->debug_callback, nullptr);
}

static void render_vulkan_create_surface(render_vulkan_t* render, void* window)
{
	VkWin32SurfaceCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	create_info.hinstance = GetModuleHandle(NULL); // TODO: not compatible with DLLs
	create_info.hwnd = (HWND)window;

	VkResult res = vkCreateWin32SurfaceKHR(render->instance, &create_info, nullptr, &render->main_surface);
	ASSERT(res == VK_SUCCESS, "Failed to create main surface");

	RECT rect;
	GetClientRect((HWND)window, &rect);
	render->extent.height = rect.right - rect.left;
	render->extent.height = rect.bottom - rect.top;

}

static void render_vulkan_destroy_surface(render_vulkan_t* render)
{
	vkDestroySurfaceKHR(render->instance, render->main_surface, nullptr);
}
	
static void render_vulkan_create_device(render_vulkan_t* render)
{
	uint32_t num_physical_devices = 0;
	VkResult res = vkEnumeratePhysicalDevices(render->instance, &num_physical_devices, nullptr);
	ASSERT(res == VK_SUCCESS && num_physical_devices > 0, "Failed to get physical device count");

	scoped_array_t<VkPhysicalDevice> physical_devices(render->allocator, num_physical_devices);

	res = vkEnumeratePhysicalDevices(render->instance, &num_physical_devices, physical_devices.begin());
	ASSERT(res == VK_SUCCESS, "Failed to enumerate physical devices");

	render->physical_device = VK_NULL_HANDLE;
	for (auto& physical_device : physical_devices)
	{
		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

		scoped_array_t<VkQueueFamilyProperties> queue_families(render->allocator, queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.begin());

		int graphics_family = -1;
		int present_family = -1;
		int i = 0;
		for (const auto& queue_family : queue_families) {
			if (queue_family.queueCount > 0 && queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				graphics_family = i;

			VkBool32 present_support = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, render->main_surface, &present_support);

			if (queue_family.queueCount > 0 && present_support) {
				present_family = i;
			}

			if (graphics_family != -1 && present_family != -1)
				break;

			i++;
		}

		if (graphics_family != -1 && present_family != -1)
		{
			render->physical_device = physical_device;
			render->graphics_queue_index = graphics_family;
			render->present_queue_index = present_family;
			break;
		}
	}
	
	ASSERT(render->physical_device != VK_NULL_HANDLE);

	VkDeviceQueueCreateInfo queue_create_info = {};
	queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.queueFamilyIndex = render->graphics_queue_index;
	queue_create_info.queueCount = 1;

	float queue_priority = 1.0f;
	queue_create_info.pQueuePriorities = &queue_priority;

	VkPhysicalDeviceFeatures device_features = {};

	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	create_info.pQueueCreateInfos = &queue_create_info;
	create_info.queueCreateInfoCount = 1;

	create_info.pEnabledFeatures = &device_features;

	// TODO: extension selection
	const char* wanted_extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	create_info.enabledExtensionCount = ARRAY_LENGTH(wanted_extensions);
	create_info.ppEnabledExtensionNames = wanted_extensions;

	// TODO: layer selection
	create_info.enabledLayerCount = 0;
	create_info.ppEnabledLayerNames = nullptr;

	res = vkCreateDevice(render->physical_device, &create_info, nullptr, &render->device);
	ASSERT(res == VK_SUCCESS);

	vkGetDeviceQueue(render->device, render->graphics_queue_index, 0, &render->graphics_queue);
	vkGetDeviceQueue(render->device, render->present_queue_index, 0, &render->present_queue);
}

static void render_vulkan_destroy_device(render_vulkan_t* render)
{
	vkDestroyDevice(render->device, nullptr);
}

static void render_vulkan_create_swapchain(render_vulkan_t* render)
{
	VkSurfaceCapabilitiesKHR capabilities = {};
	VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(render->physical_device, render->main_surface, &capabilities);
	ASSERT(res == VK_SUCCESS);

	uint32_t format_count = 0;
	res = vkGetPhysicalDeviceSurfaceFormatsKHR(render->physical_device, render->main_surface, &format_count, nullptr);
	ASSERT(res == VK_SUCCESS && format_count > 0);

	scoped_array_t<VkSurfaceFormatKHR> formats(render->allocator, format_count);
	res = vkGetPhysicalDeviceSurfaceFormatsKHR(render->physical_device, render->main_surface, &format_count, formats.begin());
	ASSERT(res == VK_SUCCESS);

	uint32_t present_mode_count = 0;
	res = vkGetPhysicalDeviceSurfacePresentModesKHR(render->physical_device, render->main_surface, &present_mode_count, nullptr);
	ASSERT(res == VK_SUCCESS && present_mode_count > 0);

	scoped_array_t<VkPresentModeKHR> present_modes(render->allocator, present_mode_count);
	res = vkGetPhysicalDeviceSurfacePresentModesKHR(render->physical_device, render->main_surface, &present_mode_count, present_modes.begin());
	ASSERT(res == VK_SUCCESS);

	VkSurfaceFormatKHR selected_format = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	bool format_found = false;
	for (const auto& format : formats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			selected_format = format;
			format_found = true;
			break;
		}
	}

	if (!format_found && formats.length() > 0)
	{
		selected_format = formats[0];
	}

	VkPresentModeKHR selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;

	for (const auto& present_mode : present_modes)
	{
		if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			selected_present_mode = present_mode;
			break;
		}
		else if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		{
			selected_present_mode = present_mode;
		}
	}

	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		render->extent = capabilities.currentExtent;
	}
	else
	{
		render->extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, render->extent.width));
		render->extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, render->extent.height));
	}

	uint32_t image_count = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount)
	{
		image_count = capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = render->main_surface;

	create_info.minImageCount = image_count;
	create_info.imageFormat = selected_format.format;
	create_info.imageColorSpace = selected_format.colorSpace;
	create_info.imageExtent = render->extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	uint32_t queue_family_indices[] = { (uint32_t)render->graphics_queue_index, (uint32_t)render->present_queue_index };

	if (render->graphics_queue_index != render->present_queue_index)
	{
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_family_indices;
	}
	else
	{
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	create_info.preTransform = capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = selected_present_mode;
	create_info.clipped = VK_TRUE;

	create_info.oldSwapchain = VK_NULL_HANDLE;

	res = vkCreateSwapchainKHR(render->device, &create_info, nullptr, &render->swap_chain);
	ASSERT(res == VK_SUCCESS);
	render->swap_chain_image_format = selected_format.format;

	res = vkGetSwapchainImagesKHR(render->device, render->swap_chain, &image_count, nullptr);
	ASSERT(res == VK_SUCCESS);
	render->swap_chain_images.create_with_length(render->allocator, image_count);
	res = vkGetSwapchainImagesKHR(render->device, render->swap_chain, &image_count, render->swap_chain_images.begin());
	ASSERT(res == VK_SUCCESS);

	render->swap_chain_image_views.create_with_length(render->allocator, image_count);

	for (size_t i = 0; i < image_count; i++)
	{
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = render->swap_chain_images[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = selected_format.format;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		res = vkCreateImageView(render->device, &createInfo, nullptr, &render->swap_chain_image_views[i]);
		ASSERT(res == VK_SUCCESS);
	}

	VkSemaphoreCreateInfo semaphore_create_info = {};
	semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphore_create_info.flags = 0;
	res = vkCreateSemaphore(render->device, &semaphore_create_info, nullptr, &render->backbuffer_semaphore);
	ASSERT(res == VK_SUCCESS, "Failed to create semaphore");

	res = vkCreateSemaphore(render->device, &semaphore_create_info, nullptr, &render->present_semaphore);
	ASSERT(res == VK_SUCCESS, "Failed to create semaphore");
}

static void render_vulkan_destroy_swapchain(render_vulkan_t* render)
{
	vkDestroySemaphore(render->device, render->present_semaphore, nullptr);
	vkDestroySemaphore(render->device, render->backbuffer_semaphore, nullptr);
	for (const auto& image_view : render->swap_chain_image_views)
		vkDestroyImageView(render->device, image_view, nullptr);
	render->swap_chain_image_views.destroy(render->allocator);
	render->swap_chain_images.destroy(render->allocator);
	vkDestroySwapchainKHR(render->device, render->swap_chain, nullptr);
}

static void render_vulkan_create_framebuffers(render_vulkan_t* render)
{
	render->swap_chain_framebuffers.create_with_length(render->allocator, render->swap_chain_images.length());
	for (size_t i = 0; i < render->swap_chain_framebuffers.length(); i++) {
		VkImageView attachments[] = {
			render->swap_chain_image_views[i]
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = render->temp_render_pass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = render->extent.width;
		framebufferInfo.height = render->extent.height;
		framebufferInfo.layers = 1;

		VkResult res = vkCreateFramebuffer(render->device, &framebufferInfo, nullptr, &render->swap_chain_framebuffers[i]);
		ASSERT(res == VK_SUCCESS);
	}
}

static void render_vulkan_destroy_framebuffers(render_vulkan_t* render)
{
	for (const auto& framebuffer : render->swap_chain_framebuffers)
		vkDestroyFramebuffer(render->device, framebuffer, nullptr);
	render->swap_chain_framebuffers.destroy(render->allocator);
}

render_result_t render_vulkan_create(const render_create_info_t* create_info, render_vulkan_t** out_render)
{
	render_vulkan_t* render = ALLOCATOR_NEW(create_info->allocator, render_vulkan_t);
	render->allocator = create_info->allocator;
	render->backend = RENDER_BACKEND_VULKAN;

	render->views.create(render->allocator, 1);
	render->scripts.create(render->allocator, 1);
	render->textures.create(render->allocator, create_info->max_textures);
	render->shaders.create(render->allocator, create_info->max_shaders);
	render->materials.create(render->allocator, create_info->max_materials);
	render->meshes.create(render->allocator, create_info->max_meshes);
	render->instances.create(render->allocator, create_info->max_instances);

	render_vulkan_create_instance(render);
	render_vulkan_create_debug_report(render);
	render_vulkan_create_surface(render, create_info->window);
	render_vulkan_create_device(render);
	render_vulkan_create_swapchain(render);

	render->command_pools.create_with_length(render->allocator, render->swap_chain_images.length());

	VkCommandPoolCreateInfo command_pool_create_info = {};
	command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_create_info.queueFamilyIndex = render->graphics_queue_index;
	command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	for (auto& command_pool : render->command_pools)
		vkCreateCommandPool(render->device, &command_pool_create_info, nullptr, &command_pool);

	render->frame_fences.create_with_length(render->allocator, render->swap_chain_images.length());

	VkFenceCreateInfo fence_create_info = {};
	fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (auto& fence : render->frame_fences)
		vkCreateFence(render->device, &fence_create_info, nullptr, &fence);

	VkAttachmentDescription color_attachment = {};
	color_attachment.format = render->swap_chain_image_format;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref = {};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &color_attachment;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;

	VkResult res = vkCreateRenderPass(render->device, &render_pass_info, nullptr, &render->temp_render_pass);
	ASSERT(res == VK_SUCCESS, "Failed to create render pass");
		
	VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[] =
	{
		0, //binding
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		1, // count
		VK_SHADER_STAGE_ALL_GRAPHICS,
		nullptr, // static samplers
	};

	VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		ARRAY_LENGTH(descriptor_set_layout_bindings),
		descriptor_set_layout_bindings,
	};
	res = vkCreateDescriptorSetLayout(render->device, &descriptor_set_layout_create_info, nullptr, &render->layout_per_frame);
	ASSERT(res == VK_SUCCESS, "Failed to descriptor set layout");

	VkDescriptorSetLayout layouts[] =
	{
		render->layout_per_frame,
};
	VkPushConstantRange push_constants[] = 
	{
		{
			VK_SHADER_STAGE_ALL_GRAPHICS,
			0,
			2 * sizeof(uint32_t),
		},
	};
	VkPipelineLayoutCreateInfo pipeline_layout_create_info =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		0,
		nullptr,
		ARRAY_LENGTH(push_constants),
		push_constants,
	};
	res = vkCreatePipelineLayout(render->device, &pipeline_layout_create_info, nullptr, &render->temp_pipeline_layout);
	ASSERT(res == VK_SUCCESS, "Failed to create pipeline layout");

	VkPipelineCacheCreateInfo pipeline_cache_create_info =
	{
		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
		nullptr,
		0,
		0,
		nullptr,
	};
	res = vkCreatePipelineCache(render->device, &pipeline_cache_create_info, nullptr, &render->pipeline_cache);
	ASSERT(res == VK_SUCCESS, "Failed to create pipeline cache");

	render_vulkan_create_framebuffers(render);

	*out_render = render;
	return RENDER_RESULT_OK;
}

void render_vulkan_destroy(render_vulkan_t* render)
{
	render_vulkan_destroy_framebuffers(render);

	vkDestroyPipelineCache(render->device, render->pipeline_cache, nullptr);
	vkDestroyDescriptorSetLayout(render->device, render->layout_per_frame, nullptr);
	vkDestroyPipelineLayout(render->device, render->temp_pipeline_layout, nullptr);
	vkDestroyRenderPass(render->device, render->temp_render_pass, nullptr);

	for (const auto& fence : render->frame_fences)
		vkDestroyFence(render->device, fence, nullptr);
	render->frame_fences.destroy(render->allocator);

	for (const auto& command_pool : render->command_pools)
		vkDestroyCommandPool(render->device, command_pool, nullptr);
	render->command_pools.destroy(render->allocator);

	render_vulkan_destroy_swapchain(render);
	render_vulkan_destroy_device(render);
	render_vulkan_destroy_surface(render);
	render_vulkan_destroy_debug_report(render);
	render_vulkan_destroy_instance(render);

	render->views.destroy(render->allocator);
	render->scripts.destroy(render->allocator);
	render->textures.destroy(render->allocator);
	render->shaders.destroy(render->allocator);
	render->materials.destroy(render->allocator);
	render->meshes.destroy(render->allocator);
	render->instances.destroy(render->allocator);

	ALLOCATOR_DELETE(render->allocator, render_vulkan_t, render);
}

void render_vulkan_wait_idle(render_vulkan_t* render)
{
	VkResult res = vkDeviceWaitIdle(render->device);
	ASSERT(res == VK_SUCCESS);
}

render_result_t render_vulkan_view_create(render_vulkan_t* render, const render_view_create_info_t* create_info, render_view_id_t* out_view_id)
{
	ASSERT(render->views.num_free() != 0);

	render_vulkan_view_t* view = render->views.alloc();

	view->data = create_info->initial_data;
	// TODO: allocate data slot for view

	*out_view_id = render->views.pointer_to_handle(view);
	return RENDER_RESULT_OK;
}

void render_vulkan_view_destroy(render_vulkan_t* render, render_view_id_t view_id)
{
	//render_vulkan_view_t* view = render->views.handle_to_pointer(view_id);

	render->views.free_handle(view_id);
}

render_result_t render_vulkan_script_create(render_vulkan_t* render, const render_script_create_info_t* create_info, render_script_id_t* out_script_id)
{
	ASSERT(render->scripts.num_free() != 0);

	render_vulkan_script_t* script = render->scripts.alloc();

	script->num_targets = create_info->num_transient_targets;
	script->targets = ALLOCATOR_ALLOC_ARRAY(render->allocator, create_info->num_transient_targets, render_vulkan_target_t);
	for (size_t t = 0; t < create_info->num_transient_targets; ++t)
	{
		const render_target_t* src_target = &create_info->transient_targets[t];
		render_vulkan_target_t* dst_target = &script->targets[t];
		dst_target->name_hash = hash_string(src_target->name);
	}

	script->num_passes = create_info->num_passes;
	script->passes = ALLOCATOR_ALLOC_ARRAY(render->allocator, create_info->num_passes, render_vulkan_pass_t);
	for (size_t p = 0; p < create_info->num_passes; ++p)
	{
		const render_pass_t* src_pass = &create_info->passes[p];
		render_vulkan_pass_t* dst_pass = &script->passes[p];

		dst_pass->num_color_attachments = src_pass->num_color_attachments;

		if (src_pass->depth_stencil_attachment != nullptr)
		{
			dst_pass->has_depth_stencil_attachment = true;
		}
		else
		{
			dst_pass->has_depth_stencil_attachment = false;
		}

		dst_pass->num_commands = src_pass->num_commands;
		dst_pass->commands = ALLOCATOR_ALLOC_ARRAY(render->allocator, src_pass->num_commands, render_command_t);
		for (size_t c = 0; c < src_pass->num_commands; ++c)
		{
			dst_pass->commands[c] = src_pass->commands[c];
		}
	}

	*out_script_id = render->scripts.pointer_to_handle(script);
	return RENDER_RESULT_OK;
}

void render_vulkan_script_destroy(render_vulkan_t* render, render_script_id_t script_id)
{
	render->scripts.free_handle(script_id);
}

render_result_t render_vulkan_texture_create(render_vulkan_t* render, const texture_data_t* texture_data, render_texture_id_t* out_texture_id)
{
	ASSERT(render->textures.num_free() != 0);

	render_vulkan_texture_t* texture = render->textures.alloc();
	texture->width = texture_data->width;
	texture->height = texture_data->height;

	*out_texture_id = render->textures.pointer_to_handle(texture);
	return RENDER_RESULT_OK;
}

void render_vulkan_texture_destroy(render_vulkan_t* render, render_texture_id_t texture_id)
{
	render->textures.free_handle(texture_id);
}

render_result_t render_vulkan_shader_create(render_vulkan_t* render, const shader_data_t* shader_data, render_shader_id_t* out_shader_id)
{
	ASSERT(shader_data->data_vulkan != nullptr);
	ASSERT(render->shaders.num_free() != 0);

	const shader_data_vulkan_t* data = shader_data->data_vulkan;

	render_vulkan_shader_t* shader = render->shaders.alloc();
	// TODO: copy less things
	shader->num_properties = data->properties.count;
	shader->properties = ALLOCATOR_ALLOC_ARRAY(render->allocator, data->properties.count, render_vulkan_shader_property_t);
	shader->num_variants = data->variants.count;
	shader->variants = ALLOCATOR_ALLOC_ARRAY(render->allocator, data->variants.count, render_vulkan_shader_variant_t);
	memset(shader->constant_buffers, 0, sizeof(shader->constant_buffers));
	for (int i = 0; i < SHADER_FREQUENCY_MAX; ++i)
	{
		size_t stride = data->constant_buffers[i].data.count;
		shader->constant_buffers[i].stride = stride;
		shader->constant_buffers[i].default_data = ALLOCATOR_ALLOC_ARRAY(render->allocator, stride, uint8_t);
		memcpy(shader->constant_buffers[i].default_data, data->constant_buffers[i].data.data, stride);

		size_t capacity = 1024;
		size_t buffer_size = capacity * stride;
		if (buffer_size > 0)
		{
			shader->constant_buffers[i].pool.create(render->allocator, capacity);
		}
	}

	for (size_t i = 0; i < data->properties.count; ++i)
	{
		shader->properties[i].frequency = data->properties[i].frequency;
		shader->properties[i].name_hash = data->properties[i].name_hash;
		shader->properties[i].pack_offset = data->properties[i].pack_offset;
		shader->properties[i].pack_size = data->properties[i].data.count;
	}

	for (size_t i = 0; i < data->variants.count; ++i)
	{
		render_vulkan_shader_variant_t* variant = &shader->variants[i];
		scoped_array_t<VkPipelineShaderStageCreateInfo> stages(render->allocator);

		for (int stage = 0; stage < SHADER_STAGE_MAX; ++stage)
		{
			if (data->variants[i].stages[stage].program.count > 0)
			{
				VkShaderModuleCreateInfo module_info = {};
				module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				module_info.codeSize = data->variants[i].stages[stage].program.count;
				module_info.pCode = (const uint32_t*)data->variants[i].stages[stage].program.data;
				VkResult res = vkCreateShaderModule(render->device, &module_info, nullptr, &variant->shader_modules[stage]);
				ASSERT(res == VK_SUCCESS, "Failed to create shader module");

				static const VkShaderStageFlagBits flag_bits_lookup[] =
				{
					VK_SHADER_STAGE_VERTEX_BIT,
					VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
					VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
					VK_SHADER_STAGE_GEOMETRY_BIT,
					VK_SHADER_STAGE_FRAGMENT_BIT,
				};

				VkPipelineShaderStageCreateInfo stage_info = {};
				stage_info.sType =  VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				stage_info.flags = 0;
				stage_info.stage = flag_bits_lookup[stage];
				stage_info.module = variant->shader_modules[stage];
				stage_info.pName = "main";
				stage_info.pSpecializationInfo = nullptr;

				stages.grow_and_append(stage_info);
			}
		}
		VkPipelineVertexInputStateCreateInfo vertex_input_state =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			nullptr,
			0,
			0,
			nullptr,
			0,
			nullptr,
		};

		VkPipelineInputAssemblyStateCreateInfo input_assembly_state =
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			VK_FALSE,
		};

		VkPipelineViewportStateCreateInfo viewport_state =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			nullptr,
			0,
			1, // TODO: dynamic numbers of viewport
			nullptr,
			1, // TODO: dynamic numbers of scissor rects
			nullptr,
		};

		VkPipelineRasterizationStateCreateInfo rasterization_state =
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_FALSE,
			VK_FALSE,
			VK_POLYGON_MODE_FILL,
			VK_CULL_MODE_BACK_BIT,
			VK_FRONT_FACE_COUNTER_CLOCKWISE,
			VK_FALSE,
			0.0f,
			0.0f,
			0.0f,
			1.0f,
		};

		VkSampleMask sample_mask[] =
		{
			0,
		};
		VkPipelineMultisampleStateCreateInfo multisample_state =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_SAMPLE_COUNT_1_BIT,
			VK_FALSE,
			1.0f,
			sample_mask,
			VK_FALSE,
			VK_FALSE,
		};

		VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_TRUE,
			VK_TRUE,
			VK_COMPARE_OP_LESS_OR_EQUAL,
			VK_FALSE,
			VK_FALSE,
			{
				VK_STENCIL_OP_KEEP,
				VK_STENCIL_OP_KEEP,
				VK_STENCIL_OP_KEEP,
				VK_COMPARE_OP_ALWAYS,
				0,
				0,
				0,
			},
			{
				VK_STENCIL_OP_KEEP,
				VK_STENCIL_OP_KEEP,
				VK_STENCIL_OP_KEEP,
				VK_COMPARE_OP_ALWAYS,
				0,
				0,
				0,
			},
			0.0f,
			1.0f,
		};

		VkPipelineColorBlendAttachmentState attachments[] =
		{
			{
				VK_FALSE,
				VK_BLEND_FACTOR_ONE,
				VK_BLEND_FACTOR_ZERO,
				VK_BLEND_OP_ADD,
				VK_BLEND_FACTOR_ONE,
				VK_BLEND_FACTOR_ZERO,
				VK_BLEND_OP_ADD,
				VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			}
		};
		VkPipelineColorBlendStateCreateInfo color_blend_state =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_FALSE,
			VK_LOGIC_OP_CLEAR,
			ARRAY_LENGTH(attachments),
			attachments,
			{ 0.0f, 0.0f, 0.0f, 0.0f },
		};

		VkDynamicState dynamic_states[] =
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
		};
		VkPipelineDynamicStateCreateInfo dynamic_state =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			nullptr,
			0,
			ARRAY_LENGTH(dynamic_states),
			dynamic_states,
		};

		VkGraphicsPipelineCreateInfo create_info =
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			nullptr,
			0,
			(uint32_t)stages.length(),
			stages.begin(),
			&vertex_input_state,
			&input_assembly_state,
			nullptr, // tessellation_state
			&viewport_state,
			&rasterization_state,
			&multisample_state,
			&depth_stencil_state,
			&color_blend_state,
			&dynamic_state,
			render->temp_pipeline_layout,
			render->temp_render_pass,
			0, // subpass
			VK_NULL_HANDLE, // base_pipeline_handle
			0, // base_pipeline_index
		};
		VkResult res = vkCreateGraphicsPipelines(render->device, render->pipeline_cache, 1, &create_info, nullptr, &variant->pipeline);
		ASSERT(res == VK_SUCCESS, "Failed to create graphics pipeline");
	}

	*out_shader_id = render->shaders.pointer_to_handle(shader);
	return RENDER_RESULT_OK;
}

void render_vulkan_shader_destroy(render_vulkan_t* render, render_shader_id_t shader_id)
{
	render_vulkan_shader_t* shader = render->shaders.handle_to_pointer(shader_id);
	ALLOCATOR_FREE(render->allocator, shader->variants);
	ALLOCATOR_FREE(render->allocator, shader->properties);
	for (int i = 0; i < SHADER_FREQUENCY_MAX; ++i)
	{
		ALLOCATOR_FREE(render->allocator, shader->constant_buffers[i].default_data);
		shader->constant_buffers[i].pool.destroy(render->allocator);
	}

	render->shaders.free_handle(shader_id);
}

render_result_t render_vulkan_material_create(render_vulkan_t* render, const material_data_t* material_data, render_material_id_t* out_material_id)
{
	ASSERT(render->materials.num_free() != 0);

	render_vulkan_material_t* material = render->materials.alloc();
	render_vulkan_shader_t* shader = material->shader = render->shaders.handle_to_pointer(material_data->shader_name_hash);
	memset(material->constant_buffers, 0, sizeof(uint8_t*) * SHADER_FREQUENCY_MAX);

	// Copy in all material specific information
	// TODO: replace with prebaked constant buffers?
	// TODO: only allow for per pass and per instance properties
	// TODO: extract this and share between backends
	for (size_t i = 0; i < material_data->properties.count; ++i)
	{
		// Find the the property from the shader
		size_t idx = SIZE_MAX;
		// TODO: rewrite with a sorted array and binary search?
		for (size_t isp = 0; isp < shader->num_properties; ++isp)
		{
			if (shader->properties[isp].name_hash == material_data->properties[i].name_hash)
			{
				idx = isp;
				break;
			}
		}
		
		if(idx != SIZE_MAX)
		{
			// Make sure we have the constant buffer to patch
			ASSERT(shader->properties[idx].pack_size == material_data->properties[i].value.count);
			const shader_frequency_t icb = shader->properties[i].frequency;
			ASSERT(icb >= SHADER_FREQUENCY_PER_MATERIAL);
			if (material->constant_buffers[icb] == nullptr)
			{
				material->constant_buffer_ids[icb] = shader->constant_buffers[icb].pool.alloc_handle();
				material->constant_buffers[icb] = ALLOCATOR_ALLOC_ARRAY(render->allocator, shader->constant_buffers[icb].stride, uint8_t);
				memcpy(material->constant_buffers[icb], shader->constant_buffers[icb].default_data, shader->constant_buffers[icb].stride);
			}

			memcpy(material->constant_buffers[icb] + shader->properties[idx].pack_offset, material_data->properties[i].value.data, shader->properties[idx].pack_size);
		}
	}

	*out_material_id = render->materials.pointer_to_handle(material);
	return RENDER_RESULT_OK;
}

void render_vulkan_material_destroy(render_vulkan_t* render, render_material_id_t material_id)
{
	render_vulkan_material_t* material = render->materials.handle_to_pointer(material_id);

	for (int icb = 0; icb < SHADER_FREQUENCY_MAX; ++icb)
	{
		if (material->constant_buffers[icb] == nullptr)
		{
			material->shader->constant_buffers[icb].pool.free_handle(material->constant_buffer_ids[icb]);
			ALLOCATOR_FREE(render->allocator, material->constant_buffers[icb]);
		}
	}

	render->materials.free_handle(material_id);
}

render_result_t render_vulkan_mesh_create(render_vulkan_t* render, const mesh_data_t* mesh_data, render_mesh_id_t* out_mesh_id)
{
	ASSERT(render->meshes.num_free() != 0);

	render_vulkan_mesh_t* mesh = render->meshes.alloc();
	mesh->num_vertices = mesh_data->vertex_data.data.count / (sizeof(float) * 3);
	mesh->num_indices = mesh_data->index_data.data.count / sizeof(uint16_t);

	*out_mesh_id = render->meshes.pointer_to_handle(mesh);
	return RENDER_RESULT_OK;
}

void render_vulkan_mesh_destroy(render_vulkan_t* render, render_mesh_id_t mesh_id)
{
	render->meshes.free_handle(mesh_id);
}

render_result_t render_vulkan_instance_create(render_vulkan_t* render, const render_instance_create_info_t* create_info, render_instance_id_t* out_instance_id)
{
	ASSERT(render->instances.num_free() != 0);

	render_vulkan_instance_t* instance = render->instances.alloc();
	instance->mesh = render->meshes.handle_to_pointer(create_info->mesh_id);
	instance->material = render->materials.handle_to_pointer(create_info->material_id);

	instance->data = create_info->initial_data;

	*out_instance_id = render->instances.pointer_to_handle(instance);
	return RENDER_RESULT_OK;
}

void render_vulkan_instance_destroy(render_vulkan_t* render, render_instance_id_t instance_id)
{
	render->instances.free_handle(instance_id);
}

render_result_t render_vulkan_instance_set_data(render_vulkan_t* render, size_t num_instances, render_instance_id_t* instance_ids, render_instance_data_t* instance_data)
{
	// TODO: do this a more efficient way?
	// TODO: extract to shared function
	render_vulkan_instance_t* instances = render->instances.base_ptr();
	render_instance_id_t* instance_indices = render->instances.index_ptr();
	for (size_t i = 0; i < num_instances; ++i)
	{
		render_vulkan_instance_t* instance = &instances[instance_indices[instance_ids[i]]]; // :E
		memcpy(&instance->data, &instance_data[i], sizeof(instance->data));
	}

	return RENDER_RESULT_OK;
}

void render_vulkan_kick_render(render_vulkan_t* render, render_view_id_t /*view_id*/, render_script_id_t /*script_id*/)
{
	uint32_t image_index = 0;
	VkResult res = vkAcquireNextImageKHR(render->device, render->swap_chain, UINT64_MAX, render->backbuffer_semaphore, VK_NULL_HANDLE, &image_index);
	ASSERT(res == VK_SUCCESS, "Failed to acquire next swapchain image");

	do
	{
		res = vkWaitForFences(render->device, 1, &render->frame_fences[image_index], VK_TRUE, UINT64_MAX);
	} while (res == VK_TIMEOUT);
		
	vkResetFences(render->device, 1, &render->frame_fences[image_index]);

	VkCommandPool command_pool = render->command_pools[image_index];
	vkResetCommandPool(render->device, command_pool, 0);

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = command_pool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	vkAllocateCommandBuffers(render->device, &allocInfo, &command_buffer);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	vkBeginCommandBuffer(command_buffer, &begin_info);

	VkRenderPassBeginInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = render->temp_render_pass;
	render_pass_info.framebuffer = render->swap_chain_framebuffers[image_index];
	render_pass_info.renderArea.offset = { 0, 0 };
	render_pass_info.renderArea.extent = render->extent;

	VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	render_pass_info.clearValueCount = 1;
	render_pass_info.pClearValues = &clearColor;

	vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdEndRenderPass(command_buffer);

	res = vkEndCommandBuffer(command_buffer);
	ASSERT(res == VK_SUCCESS);

	VkPipelineStageFlags pipeline_stage_flags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1u;
	submit_info.pWaitSemaphores = &render->backbuffer_semaphore;
	submit_info.pWaitDstStageMask = &pipeline_stage_flags;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &render->present_semaphore;
	res = vkQueueSubmit(render->graphics_queue, 1, &submit_info, render->frame_fences[image_index]);
	ASSERT(res == VK_SUCCESS, "Failed to submit command buffer");

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &render->present_semaphore;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &render->swap_chain;
	present_info.pImageIndices = &image_index;
	present_info.pResults = nullptr;
	res = vkQueuePresentKHR(render->present_queue, &present_info);
	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
	{
		// TODO: handle swapchain recreation
		// this happens on exit too, probably because window closes while we are still drawing
	}
	else
	{
		ASSERT(res == VK_SUCCESS, "Failed to queue present");
	}
}

void render_vulkan_kick_upload(render_vulkan_t* /*render*/)
{
}
