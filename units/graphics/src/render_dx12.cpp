#include "render_dx12.h"

#include <foundation/hash.h>
#include <foundation/file.h>

#include <algorithm>
#include <dxcapi.h>

#define SAFE_RELEASE(x) if(x) (x)->Release()

template<class T>
static const T& render_dx12_max(const T& l, const T& r)
{
	return l < r ? r : l;
}

static ID3D12Resource* render_dx12_create_buffer(render_dx12_t* render, size_t num_bytes, D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_STATES initial_state, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
{
	D3D12_HEAP_PROPERTIES heap_prop =
	{
		heap_type,
		D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		D3D12_MEMORY_POOL_UNKNOWN,
		0,
		0
	};

	D3D12_RESOURCE_DESC resource_desc =
	{
		D3D12_RESOURCE_DIMENSION_BUFFER,
		0,
		num_bytes,
		1,
		1,
		1,
		DXGI_FORMAT_UNKNOWN,
		1,
		0,
		D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		flags,
	};

	ID3D12Resource* resource = nullptr;
	HRESULT hr = render->device->CreateCommittedResource(
		&heap_prop,
		D3D12_HEAP_FLAG_NONE,
		&resource_desc,
		initial_state,
		nullptr,
		IID_PPV_ARGS(&resource));
	ASSERT(SUCCEEDED(hr), "failed to create committed resource");

	return resource;
}

static render_dx12_t::frame_data_t& render_dx12_curr_frame(render_dx12_t* render)
{
	return render->frame[render->frame_no % RENDER_MULTI_BUFFERING];
}

static render_dx12_t::copy_frame_data_t& render_dx12_curr_copy_frame(render_dx12_t* render)
{
	return render->copy_frame[render->copy_frame_no % RENDER_MULTI_BUFFERING];
}

static void render_dx12_push_pending_upload(render_dx12_t* render, ID3D12Resource* resource, D3D12_RESOURCE_STATES src_state, D3D12_RESOURCE_STATES dst_state)
{
	if (render->pending_uploads.full())
		render->pending_uploads.grow(render->allocator);

	render_dx12_upload_t upload =
	{
		resource,
		src_state,
		dst_state,
		render->copy_frame_no,
	};
	render->pending_uploads.append(upload);
}

static void render_dx12_push_delay_delete(render_dx12_t* render, IUnknown* resource)
{
	render_dx12_t::frame_data_t& frame = render_dx12_curr_frame(render);
	frame.delay_delete_queue.ensure_capacity(render->allocator, frame.delay_delete_queue.length() + 1);
	frame.delay_delete_queue.append(resource);
}

static WRAPPED_GPU_POINTER CreateFallbackWrappedPointer(render_dx12_t* render, ID3D12Resource* resource, UINT bufferNumElements)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC rawBufferUavDesc = {};
	rawBufferUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	rawBufferUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
	rawBufferUavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	rawBufferUavDesc.Buffer.NumElements = bufferNumElements;

	// Only compute fallback requires a valid descriptor index when creating a wrapped pointer.
	UINT descriptorHeapIndex = 0;
	if (!render->dxr_device->UsingRaytracingDriver())
	{
		descriptorHeapIndex = render->cbv_srv_uav_pool.alloc(1); // TODO we need to free, but not for now...
		//D3D12_CPU_DESCRIPTOR_HANDLE bottomLevelDescriptor = {};

		//render->device->CreateUnorderedAccessView(resource, nullptr, &rawBufferUavDesc, bottomLevelDescriptor);
	}
	return render->dxr_device->GetWrappedPointerSimple(descriptorHeapIndex, resource->GetGPUVirtualAddress());
}

render_result_t render_dx12_create(const render_create_info_t* create_info, render_dx12_t** out_render)
{
	render_dx12_t* render = ALLOCATOR_NEW(create_info->allocator, render_dx12_t);
	render->allocator = create_info->allocator;
	render->backend = RENDER_BACKEND_DX12;

	render->views.create(render->allocator, 1);
	render->scripts.create(render->allocator, 1);
	render->textures.create(render->allocator, create_info->max_textures);
	render->shaders.create(render->allocator, create_info->max_shaders);
	render->materials.create(render->allocator, create_info->max_materials);
	render->meshes.create(render->allocator, create_info->max_meshes);
	render->instances.create(render->allocator, create_info->max_instances);
	render->pending_uploads.create(render->allocator, 32);

	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&render->dxgi_factory));
	ASSERT(SUCCEEDED(hr), "failed to create dxgi factory");

	//hr = render->dxgi_factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
	//ASSERT(SUCCEEDED(hr), "failed to get warp adapter");

	IDXGIAdapter1* adapter = nullptr;
    for (UINT adapter_id = 0; DXGI_ERROR_NOT_FOUND != render->dxgi_factory->EnumAdapters1(adapter_id, &adapter); ++adapter_id)
    {
        DXGI_ADAPTER_DESC1 desc;
        hr = adapter->GetDesc1(&desc);
		ASSERT(SUCCEEDED(hr), "failed to enumerate adapter");

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

		UUID experimental_features[] = { D3D12ExperimentalShaderModels /*, D3D12RaytracingPrototype*/ };
		hr = D3D12EnableExperimentalFeatures(ARRAY_LENGTH(experimental_features), experimental_features, NULL, NULL);
		ASSERT(SUCCEEDED(hr));

		hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&render->device));
		ASSERT(SUCCEEDED(hr));

		//hr = render->device->QueryInterface(IID_PPV_ARGS(&render->dxr_device));
		//ASSERT(SUCCEEDED(hr)); // TODO: don't assert, set caps

		hr = D3D12CreateRaytracingFallbackDevice(render->device, CreateRaytracingFallbackDeviceFlags::None, 0, IID_PPV_ARGS(&render->dxr_device));
		ASSERT(SUCCEEDED(hr));

		if(render->device)
            break;
	}
	ASSERT(render->device != nullptr);

	D3D12_COMMAND_QUEUE_DESC queue_desc =
	{
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		D3D12_COMMAND_QUEUE_FLAG_NONE,
		0,
	};
	hr = render->device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&render->queue));
	ASSERT(SUCCEEDED(hr), "failed to create graphics command queue");
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	hr = render->device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&render->copy_queue));
	ASSERT(SUCCEEDED(hr), "failed to create graphics command queue");

	hr = render->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&render->fence));
	ASSERT(SUCCEEDED(hr), "failed to create frame fence");
	hr = render->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&render->copy_fence));
	ASSERT(SUCCEEDED(hr), "failed to create frame fence");

	render->event = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
	ASSERT(render->event != nullptr, "failed to create frame event");

	render->frame_no = 1;
	render->copy_frame_no = 1;

	for (int i = 0; i < RENDER_MULTI_BUFFERING; ++i)
	{
		render_dx12_t::frame_data_t& frame = render->frame[i];
		frame.fence_value = 0;
		hr = render->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.allocator));
		ASSERT(SUCCEEDED(hr), "failed to create graphics command allocator");
		hr = render->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame.allocator, nullptr, IID_PPV_ARGS(&frame.command_list));
		ASSERT(SUCCEEDED(hr), "failed to create graphics command list");
		frame.command_list->Close();
		//hr = frame.command_list->QueryInterface(IID_PPV_ARGS(&frame.dxr_command_list));
		//ASSERT(SUCCEEDED(hr), "failed to get raytracing command list");
		render->dxr_device->QueryRaytracingCommandList(frame.command_list, IID_PPV_ARGS(&frame.dxr_command_list));

		frame.delay_delete_queue.create(render->allocator, 8);
		frame.upload_fence_mark = 0;
	}

	for (int i = 0; i < RENDER_MULTI_BUFFERING; ++i)
	{
		render_dx12_t::copy_frame_data_t& frame = render->copy_frame[i];
		frame.fence_value = 0;
		hr = render->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&frame.allocator));
		ASSERT(SUCCEEDED(hr), "failed to create copy command allocator");
		hr = render->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, frame.allocator, nullptr, IID_PPV_ARGS(&frame.command_list));
		ASSERT(SUCCEEDED(hr), "failed to create copy command list");
		if(i != render->copy_frame_no) // Keep the first copy frame command list alive so we can start uploads at once
			frame.command_list->Close();

		frame.upload_fence_mark = 0;
	}

	render->rtv_size = render->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	render->dsv_size = render->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	render->cbv_srv_uav_size = render->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	render->smp_size = render->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		1024,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		0,
	};
	hr = render->device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&render->rtv_heap));
	ASSERT(SUCCEEDED(hr), "failed to create descriptor heap (RTV)");
	render->rtv_pool.create(render->allocator, rtv_heap_desc.NumDescriptors);

	D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		1024,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		0,
	};
	hr = render->device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&render->dsv_heap));
	ASSERT(SUCCEEDED(hr), "failed to create descriptor heap (DSV)");
	render->dsv_pool.create(render->allocator, dsv_heap_desc.NumDescriptors);

	D3D12_DESCRIPTOR_HEAP_DESC cbv_srv_uav_heap_desc =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		1000000,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		0,
	};
	hr = render->device->CreateDescriptorHeap(&cbv_srv_uav_heap_desc, IID_PPV_ARGS(&render->cbv_srv_uav_heap));
	ASSERT(SUCCEEDED(hr), "failed to create descriptor heap (CBV/SRV/UAV)");
	render->cbv_srv_uav_pool.create(render->allocator, cbv_srv_uav_heap_desc.NumDescriptors);

	D3D12_DESCRIPTOR_HEAP_DESC smp_heap_desc =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
		2048,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		0,
	};
	hr = render->device->CreateDescriptorHeap(&smp_heap_desc, IID_PPV_ARGS(&render->smp_heap));
	ASSERT(SUCCEEDED(hr), "failed to create descriptor heap (SMP)");
	render->smp_pool.create(render->allocator, smp_heap_desc.NumDescriptors);

	RECT rect;
	GetClientRect((HWND)create_info->window, &rect);
	render->width = rect.right - rect.left;
	render->height = rect.bottom - rect.top;

	DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
	swap_chain_desc.BufferCount = RENDER_MULTI_BUFFERING;
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swap_chain_desc.OutputWindow = (HWND)create_info->window;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.Windowed = TRUE;
	hr = render->dxgi_factory->CreateSwapChain(render->queue, &swap_chain_desc, (IDXGISwapChain**)&render->swapchain);
	ASSERT(SUCCEEDED(hr), "failed to create swapchain");

	for (int i = 0; i < RENDER_MULTI_BUFFERING; ++i)
	{
		render_dx12_t::backbuffer_data_t& backbuffer = render->backbuffer[i];
		hr = render->swapchain->GetBuffer(i, IID_PPV_ARGS(&backbuffer.resource));
		ASSERT(SUCCEEDED(hr), "failed to get back buffer %d", i);

		LPCWSTR backbuffer_name = L"back buffer";
		hr = backbuffer.resource->SetName(backbuffer_name);
		ASSERT(SUCCEEDED(hr), "failed to set back buffer name");
	}

	size_t upload_buffer_size = 16 * 1024 * 1024;
	render->upload_resource = render_dx12_create_buffer(render, upload_buffer_size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	render->upload_resource->SetName(L"upload");
	render->copy_upload_resource = render_dx12_create_buffer(render, upload_buffer_size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	render->copy_upload_resource->SetName(L"copy upload");

	D3D12_RANGE upload_range = { 0, upload_buffer_size };
	hr = render->upload_resource->Map(0, &upload_range, (void**)&render->upload_buffer);
	ASSERT(SUCCEEDED(hr), "failed to map upload buffer");
	hr = render->copy_upload_resource->Map(0, &upload_range, (void**)&render->copy_upload_buffer);
	ASSERT(SUCCEEDED(hr), "failed to map copy upload buffer");

	render->upload_ring.create(upload_buffer_size);
	render->copy_upload_ring.create(upload_buffer_size);

	render->argument_buffer = render_dx12_create_buffer(render, create_info->max_instances * sizeof(render_dx12_indirect_argument_t), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
	render->argument_buffer->SetName(L"argument buffer");
	render->instance_buffer = render_dx12_create_buffer(render, create_info->max_instances * 16 * sizeof(float), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	render->instance_buffer->SetName(L"instance buffer");

	D3D12_ROOT_PARAMETER1 root_parameters[3];

	root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	root_parameters[0].Constants.ShaderRegister = 0;
	root_parameters[0].Constants.RegisterSpace = 0;
	root_parameters[0].Constants.Num32BitValues = 1;
	root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	root_parameters[1].Descriptor.ShaderRegister = 1;
	root_parameters[1].Descriptor.RegisterSpace = 0;
	root_parameters[1].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
	root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	root_parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[2].Descriptor.ShaderRegister = 2;
	root_parameters[2].Descriptor.RegisterSpace = 0;
	root_parameters[2].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
	root_parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC1 root_desc =
	{
		ARRAY_LENGTH(root_parameters),
		root_parameters,
		0,
		nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
	};

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_desc;
	versioned_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_desc.Desc_1_1 = root_desc;

	ID3DBlob* out_blob = nullptr;
	ID3DBlob* error_blob = nullptr;
	hr = D3D12SerializeVersionedRootSignature(
		&versioned_desc,
		&out_blob,
		&error_blob);
	const char* error_msg = nullptr;
	if (error_blob)
		error_msg = (const char*)error_blob->GetBufferPointer();
	ASSERT(SUCCEEDED(hr), "failed to serialized root signature:\n%s", error_msg);

	// TODO: hash and cache signature to avoid changes when possible
	hr = render->device->CreateRootSignature(
		0,
		out_blob->GetBufferPointer(),
		out_blob->GetBufferSize(),
		IID_PPV_ARGS(&render->root_signature));
	ASSERT(SUCCEEDED(hr), "failed to create root signature");

	SAFE_RELEASE(error_blob);
	SAFE_RELEASE(out_blob);

	// TODO: hash and cache indirect args too
	D3D12_INDIRECT_ARGUMENT_DESC draw_indirect_args[2];
	draw_indirect_args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
	draw_indirect_args[0].Constant.RootParameterIndex = 0;
	draw_indirect_args[0].Constant.DestOffsetIn32BitValues = 0;
	draw_indirect_args[0].Constant.Num32BitValuesToSet = 1;
	draw_indirect_args[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	D3D12_COMMAND_SIGNATURE_DESC draw_indirect_desc =
	{
		sizeof(render_dx12_indirect_argument_t),
		ARRAY_LENGTH(draw_indirect_args),
		draw_indirect_args,
		0, // node mask
	};
	hr = render->device->CreateCommandSignature(&draw_indirect_desc, render->root_signature, IID_PPV_ARGS(&render->command_signature));
	ASSERT(SUCCEEDED(hr), "failed to create draw indirect command signature");

	render->dxr_bottom_level_as = nullptr;
	render->dxr_top_level_as = nullptr;
	
	*out_render = render;
	return RENDER_RESULT_OK;
}

void render_dx12_destroy(render_dx12_t* render)
{
	render->command_signature->Release();
	render->root_signature->Release();
	render->instance_buffer->Release();
	render->argument_buffer->Release();
	render->swapchain->Release();
	render->copy_upload_resource->Release();
	render->upload_resource->Release();
	render->smp_heap->Release();
	render->cbv_srv_uav_heap->Release();
	render->dsv_heap->Release();
	render->rtv_heap->Release();
	render->device->Release();
	render->dxgi_factory->Release();

	render->views.destroy(render->allocator);
	render->scripts.destroy(render->allocator);
	render->textures.destroy(render->allocator);
	render->shaders.destroy(render->allocator);
	render->materials.destroy(render->allocator);
	render->meshes.destroy(render->allocator);
	render->instances.destroy(render->allocator);
	render->pending_uploads.destroy(render->allocator);

	for (int i = 0; i < RENDER_MULTI_BUFFERING; ++i)
	{
		render_dx12_t::frame_data_t& frame = render->frame[i];
		frame.delay_delete_queue.destroy(render->allocator);
	}
	render->rtv_pool.destroy(render->allocator);
	render->dsv_pool.destroy(render->allocator);
	render->cbv_srv_uav_pool.destroy(render->allocator);
	render->smp_pool.destroy(render->allocator);

	ALLOCATOR_DELETE(render->allocator, render_dx12_t, render);
}

void render_dx12_wait_idle(render_dx12_t* render)
{
	// Fence on all operations
	UINT64 next_fence = render->frame_no + 1;
	HRESULT hr = render->queue->Signal(render->fence, next_fence);
	ASSERT(SUCCEEDED(hr), "failed to signal fence");

	UINT64 next_copy_fence = render->copy_frame_no + 1;
	hr = render->copy_queue->Signal(render->fence, next_copy_fence);
	ASSERT(SUCCEEDED(hr), "failed to signal fence");

	UINT64 last_completed_fence = render->fence->GetCompletedValue();
	if (last_completed_fence <= next_fence)
	{
		hr = render->fence->SetEventOnCompletion(next_fence, render->event);
		ASSERT(SUCCEEDED(hr), "failed to set event on completion");
		WaitForSingleObject(render->event, INFINITE);
	}

	UINT64 last_completed_copy_fence = render->copy_fence->GetCompletedValue();
	if (last_completed_copy_fence <= next_copy_fence)
	{
		hr = render->fence->SetEventOnCompletion(next_copy_fence, render->event);
		ASSERT(SUCCEEDED(hr), "failed to set event on completion");
		WaitForSingleObject(render->event, INFINITE);
	}
}

render_result_t render_dx12_view_create(render_dx12_t* render, const render_view_create_info_t* create_info, render_view_id_t* out_view_id)
{
	ASSERT(render->views.num_free() != 0);

	render_dx12_view_t* view = render->views.alloc();

	view->constant_buffer = render_dx12_create_buffer(render, ALIGN_UP(sizeof(render_view_data_t), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	view->constant_buffer->SetName(L"view constant buffer");
	view->data = create_info->initial_data;

	*out_view_id = render->views.pointer_to_handle(view);
	return RENDER_RESULT_OK;
}

void render_dx12_view_destroy(render_dx12_t* render, render_view_id_t view_id)
{
	render_dx12_view_t* view = render->views.handle_to_pointer(view_id);
	view->constant_buffer->Release();

	render->views.free_handle(view_id);
}

static size_t render_dx12_lookup_attachment(render_dx12_t* /*render*/, size_t num_targets, render_dx12_target_t* targets, const char* name)
{
	ASSERT(num_targets < (UINT32_MAX - 1)); // UINT32_MAX == no target bound, UINT32_MAX-1 == backbuffer bound
	// TODO: handle non transient render targets

	if(name == nullptr)
		return UINT32_MAX;

	uint32_t name_hash = hash_string(name);
	if (name_hash == hash_string("back buffer"))
		return UINT32_MAX - 1;
	for (size_t i = 0; i < num_targets; ++i)
	{
		if (targets[i].name_hash == name_hash)
			return i;
	}

	return UINT32_MAX;
}

static DXGI_FORMAT render_dx12_convert_format(render_format_t format)
{
	switch (format)
	{
	case RENDER_FORMAT_NONE:
		return DXGI_FORMAT_UNKNOWN;
	case RENDER_FORMAT_R8G8B8A8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case RENDER_FORMAT_R8G8B8A8_SRGB:
		return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	case RENDER_FORMAT_D32_FLOAT_S8X24_UINT:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

static bool render_is_depth_stencil_format(render_format_t format)
{
	return format == RENDER_FORMAT_D32_FLOAT_S8X24_UINT;
}

static UINT64 render_dx12_calc_resource_size(render_target_size_mode_t size_mode, float size, UINT64 base)
{
	if (size_mode == RENDER_TARGET_SIZE_MODE_BACKBUFFER_RELATIVE)
		return static_cast<UINT64>(size * base);
	else
		return static_cast<UINT64>(size);
}

render_result_t render_dx12_script_create(render_dx12_t* render, const render_script_create_info_t* create_info, render_script_id_t* out_script_id)
{
	ASSERT(render->scripts.num_free() != 0);

	render_dx12_script_t* script = render->scripts.alloc();

	// TODO: do this whole dance buildtime and store as platform specific dl data?
	script->num_targets = create_info->num_transient_targets;
	script->targets = ALLOCATOR_ALLOC_ARRAY(render->allocator, create_info->num_transient_targets, render_dx12_target_t);
	for (size_t t = 0; t < create_info->num_transient_targets; ++t)
	{
		const render_target_t* src_target = &create_info->transient_targets[t];
		render_dx12_target_t* dst_target = &script->targets[t];
		dst_target->name_hash = hash_string(src_target->name);

		D3D12_HEAP_PROPERTIES heap_prop =
		{
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			D3D12_MEMORY_POOL_UNKNOWN,
			0,
			0,
		};

		D3D12_RESOURCE_DESC desc =
		{
			D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			0,
			render_dx12_calc_resource_size(src_target->size_mode, src_target->width, render->width),
			(UINT)render_dx12_calc_resource_size(src_target->size_mode, src_target->height, render->height),
			1,
			1,
			render_dx12_convert_format(src_target->format),
			{ 1, 0 },
			D3D12_TEXTURE_LAYOUT_UNKNOWN,
			D3D12_RESOURCE_FLAG_NONE,
		};

		dst_target->clear_value.Format = desc.Format;
		if (render_is_depth_stencil_format(src_target->format))
		{
			dst_target->clear_value.DepthStencil.Depth = src_target->clear_value.depth_stencil.depth;
			dst_target->clear_value.DepthStencil.Stencil = src_target->clear_value.depth_stencil.stencil;
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		}
		else
		{
			memcpy(dst_target->clear_value.Color, src_target->clear_value.color.float32, sizeof(dst_target->clear_value.Color));
		}

		HRESULT hr = render->device->CreateCommittedResource(
			&heap_prop,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, // TODO:
			&dst_target->clear_value,
			IID_PPV_ARGS(&dst_target->resource));
		ASSERT(SUCCEEDED(hr), "failed to create committed resource");
	}

	script->num_passes = create_info->num_passes;
	script->passes = ALLOCATOR_ALLOC_ARRAY(render->allocator, create_info->num_passes, render_dx12_pass_t);
	for (size_t p = 0; p < create_info->num_passes; ++p)
	{
		const render_pass_t* src_pass = &create_info->passes[p];
		render_dx12_pass_t* dst_pass = &script->passes[p];

		dst_pass->num_color_attachments = src_pass->num_color_attachments;
		if (dst_pass->num_color_attachments)
		{
			for (size_t mb = 0; mb < RENDER_MULTI_BUFFERING; ++mb)
			{
				// TODO: only do this if we really reference the back buffer
				dst_pass->color_descriptor_offset[mb] = render->rtv_pool.alloc((uint32_t)src_pass->num_color_attachments);
				dst_pass->color_descriptor[mb] = render->rtv_heap->GetCPUDescriptorHandleForHeapStart();
				dst_pass->color_descriptor[mb].ptr += dst_pass->color_descriptor_offset[mb] * render->rtv_size;
			}
			for (size_t a = 0; a < src_pass->num_color_attachments; ++a)
			{
				size_t attachment = render_dx12_lookup_attachment(render, script->num_targets, script->targets, src_pass->color_attachments[a].name);

				for (size_t mb = 0; mb < RENDER_MULTI_BUFFERING; ++mb)
				{
					D3D12_CPU_DESCRIPTOR_HANDLE rtv = dst_pass->color_descriptor[mb];
					rtv.ptr += a * render->rtv_size;
					ID3D12Resource* resource = attachment != (UINT32_MAX - 1) ? script->targets[attachment].resource : render->backbuffer[mb].resource;
					render->device->CreateRenderTargetView(resource, nullptr, rtv);
				}
			}
		}
		if (src_pass->depth_stencil_attachment != nullptr)
		{
			size_t attachment = render_dx12_lookup_attachment(render, script->num_targets, script->targets, src_pass->depth_stencil_attachment->name);
			dst_pass->depth_stencil_descriptor_offset = render->dsv_pool.alloc_handle();
			dst_pass->depth_stencil_descriptor = render->dsv_heap->GetCPUDescriptorHandleForHeapStart();
			dst_pass->depth_stencil_descriptor.ptr += dst_pass->depth_stencil_descriptor_offset * render->dsv_size;
			ID3D12Resource* resource = script->targets[attachment].resource;
			render->device->CreateDepthStencilView(resource, nullptr, dst_pass->depth_stencil_descriptor);
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

void render_dx12_script_destroy(render_dx12_t* render, render_script_id_t script_id)
{
	// TODO: free everything
	render->scripts.free_handle(script_id);
}

render_result_t render_dx12_texture_create(render_dx12_t* render, const texture_data_t* texture_data, render_texture_id_t* out_texture_id)
{
	ASSERT(render->textures.num_free() != 0);

	render_dx12_texture_t* texture = render->textures.alloc();
	texture->width = texture_data->width;
	texture->height = texture_data->height;

	D3D12_RESOURCE_DESC desc =
	{
		D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		0,
		texture_data->width,
		texture_data->height,
		1,
		1,
		DXGI_FORMAT_R8G8B8A8_UNORM, // TODO
		{1, 0},
		D3D12_TEXTURE_LAYOUT_UNKNOWN,
		D3D12_RESOURCE_FLAG_NONE,
	};

	D3D12_HEAP_PROPERTIES heap_prop;
	ZeroMemory(&heap_prop, sizeof(heap_prop));
	heap_prop.Type = D3D12_HEAP_TYPE_DEFAULT;

	HRESULT hr = render->device->CreateCommittedResource(
		&heap_prop,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&texture->resource));
	ASSERT(SUCCEEDED(hr), "failed to create committed resource");

	size_t row_pitch = texture_data->pitch;
	ASSERT((row_pitch & (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)) == 0);
	size_t data_size = texture_data->data.count;
	size_t upload_offset = render->copy_upload_ring.get(data_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

	D3D12_SUBRESOURCE_FOOTPRINT pitched_desc =
	{
		desc.Format,
		texture_data->width,
		texture_data->height,
		1,
		(UINT)row_pitch,
	};

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT placed_texture =
	{
		upload_offset,
		pitched_desc,
	};

	memcpy(render->copy_upload_buffer + upload_offset, texture_data->data.data, data_size); // TODO: handle pitch

	render_dx12_t::copy_frame_data_t& frame = render_dx12_curr_copy_frame(render);

	D3D12_TEXTURE_COPY_LOCATION src;
	src.pResource = texture->resource;
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src.SubresourceIndex = 0; // TODO:

	D3D12_TEXTURE_COPY_LOCATION dst;
	dst.pResource = render->copy_upload_resource;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst.PlacedFootprint = placed_texture;

	frame.command_list->CopyTextureRegion(&src, 0, 0, 0, &dst, nullptr);

	texture->upload_fence = render->copy_frame_no;
	render_dx12_push_pending_upload(render, texture->resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	*out_texture_id = render->textures.pointer_to_handle(texture);
	return RENDER_RESULT_OK;
}

void render_dx12_texture_destroy(render_dx12_t* render, render_texture_id_t texture_id)
{
	render_dx12_texture_t* texture = render->textures.handle_to_pointer(texture_id);
	texture->resource->Release();

	render->textures.free_handle(texture_id);
}

render_result_t render_dx12_shader_create(render_dx12_t* render, const shader_data_t* shader_data, render_shader_id_t* out_shader_id)
{
	ASSERT(shader_data->data_dx12 != nullptr);
	ASSERT(render->shaders.num_free() != 0);

	const shader_data_dx12_t* data = shader_data->data_dx12;

	render_dx12_shader_t* shader = render->shaders.alloc();
	// TODO: copy less things
	shader->num_properties = data->properties.count;
	shader->properties = ALLOCATOR_ALLOC_ARRAY(render->allocator, data->properties.count, render_dx12_shader_property_t);
	shader->num_variants = data->variants.count;
	shader->variants = ALLOCATOR_ALLOC_ARRAY(render->allocator, data->variants.count, render_dx12_shader_variant_t);
	memset(shader->constant_buffers, 0, sizeof(shader->constant_buffers));

	for (int i = 0; i < SHADER_FREQUENCY_MAX; ++i)
	{
		size_t stride = data->constant_buffers[i].data.count;
		if (stride > 0)
		{
			size_t capacity = 1024;
			size_t buffer_size = capacity * stride;

			shader->constant_buffers[i].stride = stride;
			shader->constant_buffers[i].default_data = ALLOCATOR_ALLOC_ARRAY(render->allocator, stride, uint8_t);
			memcpy(shader->constant_buffers[i].default_data, data->constant_buffers[i].data.data, stride);
			shader->constant_buffers[i].pool.create(render->allocator, capacity);
			shader->constant_buffers[i].pool_buffer = render_dx12_create_buffer(render, buffer_size, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
			shader->constant_buffers[i].pool_buffer->SetName(L"pool buffer");
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
		render_dx12_shader_variant_t* variant = &shader->variants[i];

		D3D12_INPUT_ELEMENT_DESC input_layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc = {};
		pipeline_desc.pRootSignature = render->root_signature;
		pipeline_desc.VS.pShaderBytecode = data->variants[i].stages[SHADER_STAGE_VERTEX].program.data;
		pipeline_desc.VS.BytecodeLength  = data->variants[i].stages[SHADER_STAGE_VERTEX].program.count;
		pipeline_desc.HS.pShaderBytecode = data->variants[i].stages[SHADER_STAGE_HULL].program.data;
		pipeline_desc.HS.BytecodeLength  = data->variants[i].stages[SHADER_STAGE_HULL].program.count;
		pipeline_desc.DS.pShaderBytecode = data->variants[i].stages[SHADER_STAGE_DOMAIN].program.data;
		pipeline_desc.DS.BytecodeLength  = data->variants[i].stages[SHADER_STAGE_DOMAIN].program.count;
		pipeline_desc.GS.pShaderBytecode = data->variants[i].stages[SHADER_STAGE_GEOMETRY].program.data;
		pipeline_desc.GS.BytecodeLength  = data->variants[i].stages[SHADER_STAGE_GEOMETRY].program.count;
		pipeline_desc.PS.pShaderBytecode = data->variants[i].stages[SHADER_STAGE_FRAGMENT].program.data;
		pipeline_desc.PS.BytecodeLength  = data->variants[i].stages[SHADER_STAGE_FRAGMENT].program.count;

		pipeline_desc.BlendState.AlphaToCoverageEnable = FALSE;
		pipeline_desc.BlendState.IndependentBlendEnable = FALSE;
		size_t max_blend = 1;
		for (size_t j = 0; j < max_blend; ++j)
		{
			D3D12_RENDER_TARGET_BLEND_DESC* target = pipeline_desc.BlendState.RenderTarget + j;
			target->BlendEnable = FALSE;
			target->LogicOpEnable = FALSE;
			target->SrcBlend = D3D12_BLEND_ONE;
			target->DestBlend = D3D12_BLEND_ZERO;
			target->BlendOp = D3D12_BLEND_OP_ADD;
			target->SrcBlendAlpha = D3D12_BLEND_ONE;
			target->DestBlendAlpha = D3D12_BLEND_ZERO;
			target->BlendOpAlpha = D3D12_BLEND_OP_ADD;
			target->LogicOp = D3D12_LOGIC_OP_CLEAR;
			target->RenderTargetWriteMask = 0x0f;
		}

		pipeline_desc.SampleMask = 0xFFFFFFFF;

		pipeline_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		pipeline_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pipeline_desc.RasterizerState.FrontCounterClockwise = TRUE;
		pipeline_desc.RasterizerState.DepthBias = 0;
		pipeline_desc.RasterizerState.DepthBiasClamp = 0.0f;
		pipeline_desc.RasterizerState.SlopeScaledDepthBias = 0.0f;
		pipeline_desc.RasterizerState.DepthClipEnable = FALSE;
		pipeline_desc.RasterizerState.MultisampleEnable = FALSE;
		pipeline_desc.RasterizerState.AntialiasedLineEnable = FALSE;
		pipeline_desc.RasterizerState.ForcedSampleCount = 0;
		pipeline_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		pipeline_desc.DepthStencilState.DepthEnable = FALSE;
		pipeline_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		pipeline_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		pipeline_desc.DepthStencilState.StencilEnable = FALSE;
		pipeline_desc.DepthStencilState.StencilReadMask = 0;
		pipeline_desc.DepthStencilState.StencilWriteMask = 0;
		pipeline_desc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		pipeline_desc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		pipeline_desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		pipeline_desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
		pipeline_desc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		pipeline_desc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		pipeline_desc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		pipeline_desc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;

		pipeline_desc.InputLayout.pInputElementDescs = input_layout;
		pipeline_desc.InputLayout.NumElements = ARRAY_LENGTH(input_layout);

		pipeline_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
		pipeline_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		pipeline_desc.NumRenderTargets = 1;
		pipeline_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipeline_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

		pipeline_desc.SampleDesc.Count = 1;

		pipeline_desc.NodeMask = 0;

		pipeline_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		HRESULT hr = render->device->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(&variant->pipeline));
		ASSERT(SUCCEEDED(hr), "failed to create graphics pipeline state");
	}

	*out_shader_id = render->shaders.pointer_to_handle(shader);
	return RENDER_RESULT_OK;
}

void render_dx12_shader_destroy(render_dx12_t* render, render_shader_id_t shader_id)
{
	render_dx12_shader_t* shader = render->shaders.handle_to_pointer(shader_id);
	for (size_t i = 0; i < shader->num_variants; ++i)
		shader->variants[i].pipeline->Release();
	ALLOCATOR_FREE(render->allocator, shader->variants);
	ALLOCATOR_FREE(render->allocator, shader->properties);
	for (int i = 0; i < SHADER_FREQUENCY_MAX; ++i)
	{
		if (shader->constant_buffers[i].stride != 0)
		{
			ALLOCATOR_FREE(render->allocator, shader->constant_buffers[i].default_data);
			shader->constant_buffers[i].pool_buffer->Release();
			shader->constant_buffers[i].pool.destroy(render->allocator);
		}
	}

	render->shaders.free_handle(shader_id);
}

render_result_t render_dx12_material_create(render_dx12_t* render, const material_data_t* material_data, render_material_id_t* out_material_id)
{
	ASSERT(render->materials.num_free() != 0);

	render_dx12_material_t* material = render->materials.alloc();
	render_dx12_shader_t* shader = material->shader = render->shaders.handle_to_pointer(material_data->shader_name_hash);
	memset(material->constant_buffers, 0, sizeof(uint8_t*) * SHADER_FREQUENCY_MAX);

	// Copy in all material specific information
	// TODO: replace with prebaked constant buffers?
	// TODO: only allow for per pass and per instance properties
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

	material->upload_fence = 0;

	*out_material_id = render->materials.pointer_to_handle(material);
	return RENDER_RESULT_OK;
}

void render_dx12_material_destroy(render_dx12_t* render, render_material_id_t material_id)
{
	render_dx12_material_t* material = render->materials.handle_to_pointer(material_id);

	for (int icb = 0; icb < SHADER_FREQUENCY_MAX; ++icb)
	{
		if (material->constant_buffers[icb] != nullptr)
		{
			material->shader->constant_buffers[icb].pool.free_handle(material->constant_buffer_ids[icb]);
			ALLOCATOR_FREE(render->allocator, material->constant_buffers[icb]);
		}
	}

	render->materials.free_handle(material_id);
}

render_result_t render_dx12_mesh_create(render_dx12_t* render, const mesh_data_t* mesh_data, render_mesh_id_t* out_mesh_id)
{
	ASSERT(render->meshes.num_free() != 0);

	render_dx12_mesh_t* mesh = render->meshes.alloc();

	mesh->ib = render_dx12_create_buffer(render, mesh_data->index_data.data.count, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
	mesh->ib->SetName(L"ib");
	mesh->vb = render_dx12_create_buffer(render, mesh_data->vertex_data.data.count, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
	mesh->vb->SetName(L"vb");

	size_t vb_upload_offset = render->copy_upload_ring.get(mesh_data->vertex_data.data.count);
	size_t ib_upload_offset = render->copy_upload_ring.get(mesh_data->index_data.data.count);

	memcpy(render->copy_upload_buffer + vb_upload_offset, mesh_data->vertex_data.data.data, mesh_data->vertex_data.data.count);
	memcpy(render->copy_upload_buffer + ib_upload_offset, mesh_data->index_data.data.data, mesh_data->index_data.data.count);

	render_dx12_t::copy_frame_data_t& frame = render_dx12_curr_copy_frame(render);
	frame.command_list->CopyBufferRegion(mesh->vb, 0, render->copy_upload_resource, vb_upload_offset, mesh_data->vertex_data.data.count);
	frame.command_list->CopyBufferRegion(mesh->ib, 0, render->copy_upload_resource, ib_upload_offset, mesh_data->index_data.data.count);

	mesh->vb_view.BufferLocation = mesh->vb->GetGPUVirtualAddress();
	mesh->vb_view.SizeInBytes = mesh_data->vertex_data.data.count;
	mesh->vb_view.StrideInBytes = sizeof(float) * 3;

	mesh->ib_view.BufferLocation = mesh->ib->GetGPUVirtualAddress();
	mesh->ib_view.SizeInBytes = mesh_data->index_data.data.count;
	mesh->ib_view.Format = DXGI_FORMAT_R16_UINT;

	mesh->num_vertices = mesh_data->vertex_data.data.count / (sizeof(float) * 3);
	mesh->num_indices = mesh_data->index_data.data.count / sizeof(uint16_t);

	mesh->upload_fence = render->copy_frame_no;
	render_dx12_push_pending_upload(render, mesh->vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	render_dx12_push_pending_upload(render, mesh->ib, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	//mesh->dxr_vb = CreateFallbackWrappedPointer(render, mesh->vb, mesh->vb_view.SizeInBytes / mesh->vb_view.StrideInBytes);
	//mesh->dxr_ib = CreateFallbackWrappedPointer(render, mesh->ib, mesh->ib_view.SizeInBytes / mesh->ib_view.StrideInBytes);

	*out_mesh_id = render->meshes.pointer_to_handle(mesh);
	return RENDER_RESULT_OK;
}

void render_dx12_mesh_destroy(render_dx12_t* render, render_mesh_id_t mesh_id)
{
	render->meshes.free_handle(mesh_id);
}

render_result_t render_dx12_instance_create(render_dx12_t* render, const render_instance_create_info_t* create_info, render_instance_id_t* out_instance_id)
{
	ASSERT(render->instances.num_free() != 0);

	render_dx12_instance_t* instance = render->instances.alloc();
	instance->mesh = render->meshes.handle_to_pointer(create_info->mesh_id);
	instance->material = render->materials.handle_to_pointer(create_info->material_id);

	instance->data = create_info->initial_data;

	*out_instance_id = render->instances.pointer_to_handle(instance);
	return RENDER_RESULT_OK;
}

void render_dx12_instance_destroy(render_dx12_t* render, render_instance_id_t instance_id)
{
	render->instances.free_handle(instance_id);
}

render_result_t render_dx12_instance_set_data(render_dx12_t* render, size_t num_instances, render_instance_id_t* instance_ids, render_instance_data_t* instance_data)
{
	// TODO: do this a more efficient way?
	render_dx12_instance_t* instances = render->instances.base_ptr();
	render_instance_id_t* instance_indices = render->instances.index_ptr();
	for (size_t i = 0; i < num_instances; ++i)
	{
		render_dx12_instance_t* instance = &instances[instance_indices[instance_ids[i]]]; // :E
		memcpy(&instance->data, &instance_data[i], sizeof(instance->data));
	}

	return RENDER_RESULT_OK;
}

static void render_dx12_push_barrier(render_dx12_t* render, scoped_array_t<D3D12_RESOURCE_BARRIER>& barriers, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	D3D12_RESOURCE_BARRIER barrier_desc;
	barrier_desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier_desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier_desc.Transition.pResource = resource;
	barrier_desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier_desc.Transition.StateBefore = before;
	barrier_desc.Transition.StateAfter = after;
	barriers.grow_and_append(barrier_desc);
}

struct render_dx12_sort_object_t
{
	render_dx12_sort_object_t(render_shader_id_t shader_id, render_mesh_id_t mesh_id, render_instance_id_t instance_id)
	{
		lsb = instance_id;
		msb = (static_cast<uint64_t>(shader_id) << 32) | static_cast<uint64_t>(mesh_id);
	}

	uint64_t lsb, msb;

	bool operator < (const render_dx12_sort_object_t& other) { return msb < other.msb ? lsb < other.lsb : false; }
	render_mesh_id_t get_mesh_id() const { return static_cast<render_mesh_id_t>(msb & 0x0000FFFFULL); }
	render_shader_id_t get_shader_id() const { return static_cast<render_shader_id_t>((msb >> 32) & 0x0000FFFFULL); }
	render_instance_id_t get_instanceal_id() const { return static_cast<render_instance_id_t>(lsb); }
};

struct render_dx12_pass_context_t
{
	UINT64 backbuffer_index;

	size_t num_instances;
	const render_dx12_instance_t* instances;

	UINT64 passed_copy_fence;

	ID3D12GraphicsCommandList* command_list;
	ID3D12RaytracingFallbackCommandList* dxr_command_list;
};

static void render_dx12_do_draw(render_dx12_t* render, render_dx12_pass_context_t& ctx, render_dx12_pass_t* pass, render_dx12_view_t* view, render_dx12_script_t* script, size_t ip)
{
	scoped_array_t<D3D12_RESOURCE_BARRIER> barriers(render->allocator, 8); // TODO: persist between frames or temp alloc
	scoped_array_t<render_dx12_sort_object_t> sort_objects(render->allocator, ctx.num_instances); // TODO: persist between frames or temp alloc

	size_t argument_size = ctx.num_instances * sizeof(render_dx12_indirect_argument_t);
	size_t argument_data_offset = render->upload_ring.get(argument_size);
	render_dx12_indirect_argument_t* argument_data = reinterpret_cast<render_dx12_indirect_argument_t*>(render->upload_buffer + argument_data_offset);

	sort_objects.clear();
	for (size_t i = 0; i < ctx.num_instances; ++i)
	{
		const render_dx12_instance_t* instance = ctx.instances + i;
		render_dx12_mesh_t* mesh = instance->mesh;
		render_dx12_material_t* material = instance->material;
		render_dx12_shader_t* shader = material->shader;

		// Early out if mesh or material is not yet uploaded
		if (mesh->upload_fence > ctx.passed_copy_fence || material->upload_fence > ctx.passed_copy_fence)
			continue;

		// TODO: sort objects should be added once per pass if object matches what to draw
		// TODO: encode variant
		sort_objects.grow_and_append(render_dx12_sort_object_t(render->shaders.pointer_to_handle(shader), render->meshes.pointer_to_handle(mesh), (render_instance_id_t)i));
	}
	std::sort(sort_objects.begin(), sort_objects.end());

	for (size_t i = 0; i < sort_objects.length(); ++i)
	{
		render_dx12_sort_object_t& sort_object = sort_objects[i];
		const render_dx12_instance_t* instance = ctx.instances + sort_object.get_instanceal_id();
		render_dx12_mesh_t* mesh = instance->mesh;
		argument_data[i].start_index = (UINT)i; // TODO: figure out how to instance draw better later
		argument_data[i].draw_args.IndexCountPerInstance = (UINT)mesh->num_indices;
		argument_data[i].draw_args.InstanceCount = 1;
		argument_data[i].draw_args.StartIndexLocation = 0;
		argument_data[i].draw_args.BaseVertexLocation = 0;
		argument_data[i].draw_args.StartInstanceLocation = 0;
	}

	// TODO: use one argument buffer per pass, not one for whole frame
	barriers.clear();
	render_dx12_push_barrier(render, barriers, render->argument_buffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST);
	ctx.command_list->ResourceBarrier((UINT)barriers.length(), barriers.begin());

	ctx.command_list->CopyBufferRegion(render->argument_buffer, 0, render->upload_resource, argument_data_offset, argument_size);

	barriers.clear();
	render_dx12_push_barrier(render, barriers, render->argument_buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
	ctx.command_list->ResourceBarrier((UINT)barriers.length(), barriers.begin());

	ctx.command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	size_t batch_start = 0;
	render_mesh_id_t prev_mesh_id = ~0u;
	render_shader_id_t prev_shader_id = ~0u;
	for (size_t i = 0; i < sort_objects.length(); ++i)
	{
		render_dx12_sort_object_t& sort_object = sort_objects[i];
		render_mesh_id_t mesh_id = sort_object.get_mesh_id();
		render_shader_id_t shader_id = sort_object.get_shader_id();

		// If this draw is batchable just increment to next
		if (mesh_id == prev_mesh_id && shader_id == prev_shader_id)
			continue;

		// Otherwise break batch and set changed state
		size_t batch_count = i - batch_start;
		if (batch_count > 0)
		{
			size_t argument_offset = batch_start * sizeof(render_dx12_indirect_argument_t);
			ctx.command_list->ExecuteIndirect(render->command_signature, (UINT)batch_count, render->argument_buffer, argument_offset, nullptr, 0);
		}

		const render_dx12_instance_t* instance = ctx.instances + sort_object.get_instanceal_id();
		render_dx12_mesh_t* mesh = instance->mesh;
		render_dx12_material_t* material = instance->material;
		render_dx12_shader_t* shader = material->shader;

		if (shader_id != prev_shader_id)
		{
			ctx.command_list->SetPipelineState(shader->variants[0].pipeline); // TODO
			ctx.command_list->SetGraphicsRootSignature(render->root_signature);
			ctx.command_list->SetGraphicsRootShaderResourceView(1, render->instance_buffer->GetGPUVirtualAddress());
			ctx.command_list->SetGraphicsRootConstantBufferView(2, view->constant_buffer->GetGPUVirtualAddress());
			prev_shader_id = shader_id;
		}

		if (mesh_id != prev_mesh_id)
		{
			ctx.command_list->IASetIndexBuffer(&mesh->ib_view);
			ctx.command_list->IASetVertexBuffers(0, 1, &mesh->vb_view);
			prev_mesh_id = mesh_id;
		}
	}

	// Flush last batch
	size_t batch_count = sort_objects.length() - batch_start;
	if(batch_count > 0)
	{
		size_t argument_offset = batch_start * sizeof(render_dx12_indirect_argument_t);
		ctx.command_list->ExecuteIndirect(render->command_signature, (UINT)batch_count, render->argument_buffer, argument_offset, nullptr, 0);
	}
}

static void render_dx12_do_dispatch_rays(render_dx12_t* render, render_dx12_pass_context_t& ctx, render_dx12_pass_t* pass, render_dx12_view_t* view, render_dx12_script_t* script, size_t ip)
{
	D3D12_FALLBACK_DISPATCH_RAYS_DESC raytraceDesc = {};
	raytraceDesc.RayGenerationShaderRecord.StartAddress = render->dxr_shader_table->GetGPUVirtualAddress() + 0 * render->dxr_shader_table_entry_size;
	raytraceDesc.RayGenerationShaderRecord.SizeInBytes = render->dxr_shader_table_entry_size;
	raytraceDesc.Width = render->width;
	raytraceDesc.Height = render->height;

	// RayGen is the first entry in the shader-table
	raytraceDesc.RayGenerationShaderRecord.StartAddress = render->dxr_shader_table->GetGPUVirtualAddress() + 0 * render->dxr_shader_table_entry_size;
	raytraceDesc.RayGenerationShaderRecord.SizeInBytes = render->dxr_shader_table_entry_size;

	// Miss is the second entry in the shader-table
	size_t missOffset = 1 * render->dxr_shader_table_entry_size;
	raytraceDesc.MissShaderTable.StartAddress = render->dxr_shader_table->GetGPUVirtualAddress() + missOffset;
	raytraceDesc.MissShaderTable.StrideInBytes = render->dxr_shader_table_entry_size;
	raytraceDesc.MissShaderTable.SizeInBytes = render->dxr_shader_table_entry_size;   // Only a s single miss-entry

	// Hit is the third entry in the shader-table
	size_t hitOffset = 2 * render->dxr_shader_table_entry_size;
	raytraceDesc.HitGroupTable.StartAddress = render->dxr_shader_table->GetGPUVirtualAddress() + hitOffset;
	raytraceDesc.HitGroupTable.StrideInBytes = render->dxr_shader_table_entry_size;
	raytraceDesc.HitGroupTable.SizeInBytes = render->dxr_shader_table_entry_size;

	// Bind the empty root signature
	ctx.command_list->SetComputeRootSignature(render->dxr_empty_root_signature);

	ID3D12DescriptorHeap* heaps[] = { render->cbv_srv_uav_heap };
	ctx.dxr_command_list->SetDescriptorHeaps(ARRAY_LENGTH(heaps), heaps);

	ctx.command_list->SetComputeRootDescriptorTable(GlobalRootSignatureParams::VertexBuffersSlot, m_indexBuffer.gpuDescriptorHandle);
	ctx.command_list->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputViewSlot, m_raytracingOutputResourceUAVGpuDescriptor);

	ctx.dxr_command_list->SetTopLevelAccelerationStructure(GlobalRootSignatureParams::AccelerationStructureSlot, m_fallbackTopLevelAccelerationStructurePointer);

	// Dispatch
	ctx.dxr_command_list->DispatchRays(render->dxr_pipeline_state, &raytraceDesc);
}

static void render_dx12_do_pass(render_dx12_t* render, render_dx12_pass_context_t& ctx, render_dx12_view_t* view, render_dx12_script_t* script, size_t ip)
{
	render_dx12_pass_t* pass = &script->passes[ip];

	D3D12_CPU_DESCRIPTOR_HANDLE* color_targets = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE* depth_stencil_target = nullptr;
	// TODO: load actions
	if (pass->num_color_attachments)
	{
		color_targets = &pass->color_descriptor[ctx.backbuffer_index];
		for (size_t t = 0; t < pass->num_color_attachments; ++t)
		{
			const float color[4] = { 0.3f, 0.3f, 0.7f, 0.0f };
			D3D12_CPU_DESCRIPTOR_HANDLE rtv = pass->color_descriptor[ctx.backbuffer_index];
			rtv.ptr += t * render->rtv_size;
			ctx.command_list->ClearRenderTargetView(rtv, color, 0, nullptr);
		}
	}

	if (pass->has_depth_stencil_attachment)
	{
		depth_stencil_target = &pass->depth_stencil_descriptor;
		ctx.command_list->ClearDepthStencilView(pass->depth_stencil_descriptor, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	}

	// TODO: handle nullptr here if no color or depth target
	ctx.command_list->OMSetRenderTargets((UINT)pass->num_color_attachments, color_targets, true, depth_stencil_target);

	D3D12_RECT rect = { 0, 0, (LONG)render->width, (LONG)render->height }; // TODO
	ctx.command_list->RSSetScissorRects(1, &rect);

	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)render->width, (float)render->height, 0.0f, 1.0f };
	ctx.command_list->RSSetViewports(1, &viewport);

	for (size_t i = 0; i < pass->num_commands; ++i)
	{
		switch (pass->commands[i].type)
		{
			case RENDER_COMMAND_DRAW:
				render_dx12_do_draw(render, ctx, pass, view, script, ip);
				break;
			case RENDER_COMMAND_DISPATCH_RAYS:
				render_dx12_do_dispatch_rays(render, ctx, pass, view, script, ip);
				break;
			default:
				BREAKPOINT();
		}
	}
}

static void render_dx12_create_bottom_level_as(render_dx12_t* render)
{
	render_dx12_t::frame_data_t& frame = render_dx12_curr_frame(render);

	render_dx12_mesh_t* mesh = render->meshes.handle_to_pointer(0);

	D3D12_RAYTRACING_GEOMETRY_DESC geom_desc = {};
	geom_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geom_desc.Triangles.VertexBuffer.StartAddress = mesh->vb->GetGPUVirtualAddress();
	geom_desc.Triangles.VertexBuffer.StrideInBytes = 3 * sizeof(float);
	geom_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geom_desc.Triangles.VertexCount = (UINT)mesh->num_vertices;
	geom_desc.Triangles.IndexBuffer = mesh->ib->GetGPUVirtualAddress();
	geom_desc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
	geom_desc.Triangles.IndexCount = (UINT)mesh->num_indices;
	geom_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	// Get the size requirements for the scratch and AS buffers
	D3D12_GET_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO_DESC prebuild_desc = {};
	prebuild_desc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	prebuild_desc.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	prebuild_desc.NumDescs = 1;
	prebuild_desc.pGeometryDescs = &geom_desc;
	prebuild_desc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

	render->dxr_device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuild_desc, &render->dxr_as_prebuild_info);

	D3D12_RESOURCE_STATES initialResourceState = render->dxr_device->GetAccelerationStructureResourceState();

	// Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
	ID3D12Resource* scratch = render_dx12_create_buffer(render, render->dxr_as_prebuild_info.ScratchDataSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ID3D12Resource* result = render_dx12_create_buffer(render, render->dxr_as_prebuild_info.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, initialResourceState, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	// Create the bottom-level AS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
	desc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	desc.pGeometryDescs = &geom_desc;
	desc.DestAccelerationStructureData.StartAddress = result->GetGPUVirtualAddress();
	desc.DestAccelerationStructureData.SizeInBytes = render->dxr_as_prebuild_info.ResultDataMaxSizeInBytes;

	desc.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	desc.NumDescs = 1;
	desc.ScratchAccelerationStructureData.StartAddress = scratch->GetGPUVirtualAddress();
	desc.ScratchAccelerationStructureData.SizeInBytes = render->dxr_as_prebuild_info.ScratchDataSizeInBytes;

	desc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

	ID3D12DescriptorHeap* heaps[] = { render->cbv_srv_uav_heap };
	frame.dxr_command_list->SetDescriptorHeaps(ARRAY_LENGTH(heaps), heaps);
	frame.dxr_command_list->BuildRaytracingAccelerationStructure(&desc);

	// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
	D3D12_RESOURCE_BARRIER uav_barrier = {};
	uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uav_barrier.UAV.pResource = result;
	frame.command_list->ResourceBarrier(1, &uav_barrier);

	render_dx12_push_delay_delete(render, scratch);

	render->dxr_bottom_level_as = result;
}


static void render_dx12_create_top_level_as(render_dx12_t* render)
{
	render_dx12_t::frame_data_t& frame = render_dx12_curr_frame(render);

	// First, get the size of the TLAS buffers and create them
	D3D12_GET_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO_DESC prebuild_desc = {};
	prebuild_desc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	prebuild_desc.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	prebuild_desc.NumDescs = 1;
	prebuild_desc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
	render->dxr_device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuild_desc, &info);

	D3D12_RESOURCE_STATES initialResourceState = render->dxr_device->GetAccelerationStructureResourceState();

	// Create the buffers
	ID3D12Resource* scratch = render_dx12_create_buffer(render, info.ScratchDataSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ID3D12Resource* result = render_dx12_create_buffer(render, info.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, initialResourceState, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	//tlasSize = info.ResultDataMaxSizeInBytes;

	// The instance desc should be inside a buffer, create and map the buffer
	ID3D12Resource* instance_desc_buffer = render_dx12_create_buffer(render, sizeof(D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC* instance_desc;
	instance_desc_buffer->Map(0, nullptr, (void**)&instance_desc);

	// Initialize the instance desc. We only have a single instance
	instance_desc->InstanceID = 0;                            // This value will be exposed to the shader via InstanceID()
	instance_desc->InstanceContributionToHitGroupIndex = 0;   // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
	instance_desc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
	float m[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
	memcpy(instance_desc->Transform, &m, sizeof(instance_desc->Transform));

	UINT numBufferElements = static_cast<UINT>(render->dxr_as_prebuild_info.ResultDataMaxSizeInBytes) / sizeof(UINT32);
	instance_desc->AccelerationStructure = CreateFallbackWrappedPointer(render, render->dxr_bottom_level_as, numBufferElements);
	instance_desc->InstanceMask = 0xFF;

	// Unmap
	instance_desc_buffer->Unmap(0, nullptr);

	// Create the TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
	desc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	desc.DestAccelerationStructureData.StartAddress = result->GetGPUVirtualAddress();
	desc.DestAccelerationStructureData.SizeInBytes = info.ResultDataMaxSizeInBytes;

	desc.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	desc.InstanceDescs = instance_desc_buffer->GetGPUVirtualAddress();
	desc.NumDescs = 1;
	desc.ScratchAccelerationStructureData.StartAddress = scratch->GetGPUVirtualAddress();
	desc.ScratchAccelerationStructureData.SizeInBytes = info.ScratchDataSizeInBytes;
	desc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	ID3D12DescriptorHeap* heaps[] = { render->cbv_srv_uav_heap };
	frame.dxr_command_list->SetDescriptorHeaps(ARRAY_LENGTH(heaps), heaps);
	frame.dxr_command_list->BuildRaytracingAccelerationStructure(&desc);

	// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
	D3D12_RESOURCE_BARRIER uav_barrier = {};
	uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uav_barrier.UAV.pResource = result;
	frame.command_list->ResourceBarrier(1, &uav_barrier);

	render_dx12_push_delay_delete(render, scratch);
	render_dx12_push_delay_delete(render, instance_desc_buffer);

	render->dxr_top_level_as = result;
}

void render_dx12_create_as(render_dx12_t* render)
{
	render_dx12_create_bottom_level_as(render);
	render_dx12_create_top_level_as(render);
}

IDxcBlob* render_dx12_compile_library(render_dx12_t* render, const char* filename, const WCHAR* target_string)
{
	IDxcCompiler* compiler;
	HRESULT hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
	ASSERT(SUCCEEDED(hr));

	IDxcLibrary* library;
	hr = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
	ASSERT(SUCCEEDED(hr));

	char* shader_text = file_open_read_all_text(filename, render->allocator);

	IDxcBlobEncoding* text_blob;
	hr = library->CreateBlobWithEncodingFromPinned((LPBYTE)shader_text, (UINT32)strlen(shader_text), 0, &text_blob);
	ASSERT(SUCCEEDED(hr));

	IDxcOperationResult* result;
	hr = compiler->Compile(text_blob, L"foo", L"", target_string, nullptr, 0, nullptr, 0, nullptr, &result);
	ASSERT(SUCCEEDED(hr));

	result->GetStatus(&hr);
	if (FAILED(hr))
	{
		IDxcBlobEncoding* error;
		result->GetErrorBuffer(&error);
		BREAKPOINT();
		//std::string log = convertBlobToString(pError.GetInterfacePtr());
		//msgBox("Compiler error:\n" + log);
		return nullptr;
	}

	IDxcBlob* blob;
	hr = result->GetResult(&blob);
	ASSERT(SUCCEEDED(hr));

	result->Release();
	text_blob->Release();
	ALLOCATOR_FREE(render->allocator, shader_text);
	library->Release();
	compiler->Release();

	return blob;
}

static ID3D12RootSignature* render_dx12_create_root_signature(render_dx12_t* render, const D3D12_ROOT_SIGNATURE_DESC& desc)
{
    ID3DBlob* signature_blob = nullptr;
    ID3DBlob* error_blob = nullptr;
    HRESULT hr = render->dxr_device->D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob, &error_blob);
	ASSERT(SUCCEEDED(hr));

    ID3D12RootSignature* root_signature = nullptr;
    hr = render->dxr_device->CreateRootSignature(0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature));
	ASSERT(SUCCEEDED(hr));

    return root_signature;
}


static const WCHAR* kRayGenShader = L"rayGen";
static const WCHAR* kMissShader = L"miss";
static const WCHAR* kClosestHitShader = L"chs";
static const WCHAR* kHitGroup = L"HitGroup";
static void render_dx12_create_pipeline(render_dx12_t* render)
{
	IDxcBlob* library = render_dx12_compile_library(render, "rtshader.hlsl", L"lib_6_3"); // TODO: release?

	D3D12_STATE_SUBOBJECT subobjects[10];
	size_t curr_subobject = 0;

	D3D12_EXPORT_DESC entry_points[] = {
		{kRayGenShader, nullptr, D3D12_EXPORT_FLAG_NONE},
		{kMissShader, nullptr, D3D12_EXPORT_FLAG_NONE},
		{kClosestHitShader, nullptr, D3D12_EXPORT_FLAG_NONE},
	};

	D3D12_DXIL_LIBRARY_DESC dxil_lib_desc = {};
	dxil_lib_desc.DXILLibrary.pShaderBytecode = library->GetBufferPointer();
	dxil_lib_desc.DXILLibrary.BytecodeLength = library->GetBufferSize();
	dxil_lib_desc.NumExports = ARRAY_LENGTH(entry_points);
	dxil_lib_desc.pExports = entry_points;

	D3D12_STATE_SUBOBJECT& dxil_lib_subobject = subobjects[curr_subobject++];
	dxil_lib_subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	dxil_lib_subobject.pDesc = &dxil_lib_desc;

	D3D12_HIT_GROUP_DESC hit_group_desc = {};
	hit_group_desc.ClosestHitShaderImport = kClosestHitShader;
	hit_group_desc.HitGroupExport = kHitGroup;

	D3D12_STATE_SUBOBJECT& hit_group_subobject = subobjects[curr_subobject++];
	hit_group_subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
	hit_group_subobject.pDesc = &hit_group_desc;

	D3D12_DESCRIPTOR_RANGE ranges[2] = {};
	ranges[0].BaseShaderRegister = 0;
	ranges[0].NumDescriptors = 1;
	ranges[0].RegisterSpace = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[1].BaseShaderRegister = 0;
	ranges[1].NumDescriptors = 1;
	ranges[1].RegisterSpace = 0;
	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[1].OffsetInDescriptorsFromTableStart = 1;

	D3D12_ROOT_PARAMETER root_params[1] = {};
	root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	root_params[0].DescriptorTable.NumDescriptorRanges = ARRAY_LENGTH(ranges);
	root_params[0].DescriptorTable.pDescriptorRanges = ranges;

	D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
	root_signature_desc.NumParameters = ARRAY_LENGTH(root_params);
	root_signature_desc.pParameters = root_params;
	root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

	size_t rgs_root_signature_index = curr_subobject++;
	D3D12_STATE_SUBOBJECT& rgs_root_signature_subobject = subobjects[rgs_root_signature_index];
	ID3D12RootSignature* rgs_root_signature = render_dx12_create_root_signature(render, root_signature_desc); // TODO: release?
	rgs_root_signature_subobject.pDesc = &rgs_root_signature;
	rgs_root_signature_subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;

	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION rgs_association = {};
	rgs_association.NumExports = 1;
	rgs_association.pExports = &kRayGenShader;
	rgs_association.pSubobjectToAssociate = &subobjects[rgs_root_signature_index];

	D3D12_STATE_SUBOBJECT& rgs_export_association_subobject = subobjects[curr_subobject++];
	rgs_export_association_subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	rgs_export_association_subobject.pDesc = &rgs_association;

	D3D12_ROOT_SIGNATURE_DESC hm_root_signature_desc = {};
	hm_root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

	size_t hm_root_signature_index = curr_subobject++;
	D3D12_STATE_SUBOBJECT& hm_root_signature_subobject = subobjects[hm_root_signature_index];
	ID3D12RootSignature* hm_root_signature = render_dx12_create_root_signature(render, hm_root_signature_desc); // TODO: release?
	hm_root_signature_subobject.pDesc = &hm_root_signature;
	hm_root_signature_subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;

	const WCHAR* missHitExportName[] = { kMissShader, kHitGroup };
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION hm_association = {};
	hm_association.NumExports = ARRAY_LENGTH(missHitExportName);
	hm_association.pExports = missHitExportName;
	hm_association.pSubobjectToAssociate = &subobjects[hm_root_signature_index];

	D3D12_STATE_SUBOBJECT& hm_export_association_subobject = subobjects[curr_subobject++];
	hm_export_association_subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	hm_export_association_subobject.pDesc = &hm_association;

	D3D12_RAYTRACING_SHADER_CONFIG shader_config = {};
	shader_config.MaxAttributeSizeInBytes = 2 * sizeof(float);
	shader_config.MaxPayloadSizeInBytes = 1 * sizeof(float);

	size_t shader_config_index = curr_subobject++;
	D3D12_STATE_SUBOBJECT& shader_config_subobject = subobjects[shader_config_index];
	shader_config_subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
	shader_config_subobject.pDesc = &shader_config;

	const WCHAR* sc_exports[] = { kMissShader, kHitGroup, kRayGenShader };
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION sc_association = {};
	sc_association.NumExports = ARRAY_LENGTH(sc_exports);
	sc_association.pExports = sc_exports;
	sc_association.pSubobjectToAssociate = &subobjects[shader_config_index];

	D3D12_STATE_SUBOBJECT& sc_export_association_subobject = subobjects[curr_subobject++];
	sc_export_association_subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	sc_export_association_subobject.pDesc = &sc_association;

	D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config = {};
	pipeline_config.MaxTraceRecursionDepth = 0;

	D3D12_STATE_SUBOBJECT& pipeline_config_subobject = subobjects[curr_subobject++];
	pipeline_config_subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
	pipeline_config_subobject.pDesc = &pipeline_config;

	D3D12_ROOT_SIGNATURE_DESC global_root_signature_desc = {};

	D3D12_STATE_SUBOBJECT& global_root_signature_subobject = subobjects[curr_subobject++];
	ID3D12RootSignature* global_root_signature = render_dx12_create_root_signature(render, global_root_signature_desc); // TODO: release?
	global_root_signature_subobject.pDesc = &global_root_signature;
	global_root_signature_subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
	render->dxr_empty_root_signature = global_root_signature;

	ASSERT(curr_subobject <= ARRAY_LENGTH(subobjects));
	D3D12_STATE_OBJECT_DESC state_object_desc = {};
	state_object_desc.NumSubobjects = (UINT)curr_subobject;
	state_object_desc.pSubobjects = subobjects;
	state_object_desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

	HRESULT hr = render->dxr_device->CreateStateObject(&state_object_desc, IID_PPV_ARGS(&render->dxr_pipeline_state));
	ASSERT(SUCCEEDED(hr));
}

void render_dx12_kick_render(render_dx12_t* render, render_view_id_t view_id, render_script_id_t script_id)
{
	render_dx12_view_t* view = render->views.handle_to_pointer(view_id);
	render_dx12_script_t* script = render->scripts.handle_to_pointer(script_id);

	render_dx12_pass_context_t ctx = { 0 };

	HRESULT hr;
	scoped_array_t<D3D12_RESOURCE_BARRIER> barriers(render->allocator); // TODO: persist between frames or temp alloc

	// Get current backbuffer
	ctx.backbuffer_index = render->swapchain->GetCurrentBackBufferIndex();
	render_dx12_t::backbuffer_data_t& backbuffer = render->backbuffer[ctx.backbuffer_index];

	render_dx12_push_barrier(render, barriers, backbuffer.resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	ctx.passed_copy_fence = render->copy_fence->GetCompletedValue();
	for (size_t i = 0; i < render->pending_uploads.length(); )
	{
		// TODO: we know queue is processed in order and in chunks. Do this more efficient
		render_dx12_upload_t& upload = render->pending_uploads[i];
		if (upload.upload_fence > ctx.passed_copy_fence)
		{
			render_dx12_push_barrier(render, barriers, upload.resoure, upload.src_state, upload.dst_state); // TODO: no swapping here
			render->pending_uploads.remove_at_fast(i);
		}
		else
		{
			++i;
		}
	}

	render_dx12_t::frame_data_t& frame = render_dx12_curr_frame(render);
	ctx.command_list = frame.command_list;
	ctx.dxr_command_list = frame.dxr_command_list;
	// Make sure the next frame has passed so we can start recording new commands
	// TODO: do this later, but we need it now to be able to upload data all the time
	UINT64 last_completed_fence = render->fence->GetCompletedValue();
	if (last_completed_fence < frame.fence_value)
	{
		hr = render->fence->SetEventOnCompletion(frame.fence_value, render->event);
		ASSERT(SUCCEEDED(hr), "failed to set event on completion");
		WaitForSingleObject(render->event, INFINITE);
	}

	// Reset frame
	frame.allocator->Reset();
	frame.command_list->Reset(frame.allocator, nullptr);
	for (size_t i = 0; i < frame.delay_delete_queue.length(); ++i)
		frame.delay_delete_queue[i]->Release();
	frame.delay_delete_queue.set_length(0);
	render->upload_ring.free_to_mark(frame.upload_fence_mark);

	// Do beginning of frame barriers
	frame.command_list->ResourceBarrier((UINT)barriers.length(), barriers.begin());

	// dxr start
	if (render->dxr_bottom_level_as == nullptr)
	{
		render_dx12_create_as(render);
		render_dx12_create_pipeline(render);

		uint32_t progIdSize = render->dxr_device->GetShaderIdentifierSize();
		render->dxr_shader_table_entry_size = progIdSize;
		render->dxr_shader_table_entry_size += 8; // The ray-gen's descriptor table
		render->dxr_shader_table_entry_size = ALIGN_UP(render->dxr_shader_table_entry_size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
		uint32_t shaderTableSize = render->dxr_shader_table_entry_size * 3;

		render->dxr_shader_table = render_dx12_create_buffer(render, shaderTableSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);

		uint8_t* pData;
		hr = render->dxr_shader_table->Map(0, nullptr, (void**)&pData);
		ASSERT(SUCCEEDED(hr));

		//ID3D12StateObjectPropertiesPrototype* pRtsoProps = nullptr;
		//hr = render->dxr_pipeline_state->QueryInterface(IID_PPV_ARGS(&pRtsoProps));
		//ASSERT(SUCCEEDED(hr));

		// Entry 0 - ray-gen program ID and descriptor data
		const void* ray_gen_shader_identifier = render->dxr_pipeline_state->GetShaderIdentifier(kRayGenShader);
		memcpy(pData, ray_gen_shader_identifier, progIdSize);

		// This is where we need to set the descriptor data for the ray-gen shader. We'll get to it in the next tutorial

		// Entry 1 - miss program
		memcpy(pData + render->dxr_shader_table_entry_size, render->dxr_pipeline_state->GetShaderIdentifier(kMissShader), progIdSize);

		// Entry 2 - hit program
		uint8_t* pHitEntry = pData + render->dxr_shader_table_entry_size * 2; // +2 skips the ray-gen and miss entries
		memcpy(pHitEntry, render->dxr_pipeline_state->GetShaderIdentifier(kHitGroup), progIdSize);

		// Unmap
		render->dxr_shader_table->Unmap(0, nullptr);
	}

	// Start uploading instance and view data for everything
	// TODO: don't do this for data that has not changed!
	ctx.num_instances = render->instances.num_used();
	size_t instance_size = ctx.num_instances * 16 * sizeof(float);
	size_t instance_data_offset = render->upload_ring.get(instance_size);
	float* instance_data = reinterpret_cast<float*>(render->upload_buffer + instance_data_offset);

	size_t view_size = sizeof(render_view_data_t);
	size_t view_data_offset = render->upload_ring.get(view_size);
	render_view_data_t* view_data = reinterpret_cast<render_view_data_t*>(render->upload_buffer + view_data_offset);
	*view_data = view->data; // TODO: multiple views

	ctx.instances = render->instances.base_ptr();
	for (size_t i = 0; i < ctx.num_instances; ++i)
	{
		const render_dx12_instance_t* instance = ctx.instances + i;
		//render_dx12_mesh_t* mesh = instance->mesh;
		//render_dx12_material_t* material = instance->material;
		//render_dx12_shader_t* shader = material->shader;

		memcpy(instance_data + i * 16, instance->data.transform, 16 * sizeof(float));
	}

	barriers.clear();
	render_dx12_push_barrier(render, barriers, render->instance_buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	render_dx12_push_barrier(render, barriers, view->constant_buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST);
	frame.command_list->ResourceBarrier((UINT)barriers.length(), barriers.begin());

	frame.command_list->CopyBufferRegion(render->instance_buffer, 0, render->upload_resource, instance_data_offset, instance_size);
	frame.command_list->CopyBufferRegion(view->constant_buffer, 0, render->upload_resource, view_data_offset, view_size);

	barriers.clear();
	render_dx12_push_barrier(render, barriers, render->instance_buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	render_dx12_push_barrier(render, barriers, view->constant_buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	frame.command_list->ResourceBarrier((UINT)barriers.length(), barriers.begin());

	for (size_t p = 0; p < script->num_passes; ++p)
	{
		render_dx12_do_pass(render, ctx, view, script, p);
	}

	barriers.clear();
	render_dx12_push_barrier(render, barriers, backbuffer.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	frame.command_list->ResourceBarrier((UINT)barriers.length(), barriers.begin());

	hr = frame.command_list->Close();
	ASSERT(SUCCEEDED(hr), "failed to close command list");

	ID3D12CommandList* command_lists[] =
	{
		frame.command_list,
	};
	render->queue->ExecuteCommandLists(ARRAY_LENGTH(command_lists), command_lists);

	// Present
	DXGI_PRESENT_PARAMETERS present_params;
	ZeroMemory(&present_params, sizeof(present_params));
	hr = render->swapchain->Present1(0, DXGI_PRESENT_RESTART, &present_params);
	ASSERT(SUCCEEDED(hr), "failed to present");

	// Add fence and step frame
	UINT64 next_fence = render->frame_no;
	hr = render->queue->Signal(render->fence, next_fence);
	ASSERT(SUCCEEDED(hr), "failed to signal fence");
	frame.fence_value = next_fence;
	frame.upload_fence_mark = render->upload_ring.get_mark();
	render->frame_no += 1;
}

void render_dx12_kick_upload(render_dx12_t* render)
{
	render_dx12_t::copy_frame_data_t& frame = render_dx12_curr_copy_frame(render);

	frame.command_list->Close();
	ID3D12CommandList* command_lists[] =
	{
		frame.command_list,
	};
	render->copy_queue->ExecuteCommandLists(ARRAY_LENGTH(command_lists), command_lists);

	// Add fence and step frame
	UINT64 next_fence = render->copy_frame_no;
	HRESULT hr = render->queue->Signal(render->copy_fence, next_fence);
	ASSERT(SUCCEEDED(hr), "failed to signal fence");
	frame.fence_value = next_fence;
	frame.upload_fence_mark = render->copy_upload_ring.get_mark();
	render->copy_frame_no += 1;

	// Make sure the next frame has passed so we can start recording new commands
	render_dx12_t::copy_frame_data_t& frame_next = render_dx12_curr_copy_frame(render);
	UINT64 last_completed_fence = render->copy_fence->GetCompletedValue();
	if (last_completed_fence < frame_next.fence_value)
	{
		hr = render->fence->SetEventOnCompletion(frame_next.fence_value, render->event);
		ASSERT(SUCCEEDED(hr), "failed to set event on completion");
		WaitForSingleObject(render->event, INFINITE);
	}

	frame_next.allocator->Reset();
	frame_next.command_list->Reset(frame_next.allocator, nullptr);
	render->copy_upload_ring.free_to_mark(frame_next.upload_fence_mark);
}