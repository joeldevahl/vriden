#pragma once

#include <graphics/render.h>
#include "render_common.h"

#include <foundation/objpool.h>
#include <foundation/cobjpool.h>
#include <foundation/idpool.h>
#include <foundation/range_pool.h>

#include <d3d12.h>
#include <dxgi1_4.h>

#define RENDER_MULTI_BUFFERING 2

struct ring_dx12_buffer_t
{
	size_t _get;
	size_t _put;
	size_t _size;
	
	ring_dx12_buffer_t()
	{
		create(0);
	}

	ring_dx12_buffer_t(size_t size)
	{
		create(size);
	}

	void create(size_t size)
	{
		_get = 0;
		_put = size;
		_size = size;
	}

	size_t get(size_t size)
	{
		if (_get > _put) // case where ring segment is contiguous
			if (_put - _get < size) // do size fit in range?
				BREAKPOINT();
		else // case where ring is split
			if (_size - _get < size) // do size fit in first part?
				if (_put >= size) // do we fit in second part?
					_get = 0;
				else
					BREAKPOINT();

		size_t res = _get;
		_get += size;

		// take care of _get wraparound
		if (_get > _size)
			_get -= _size;
		
		return res;
	}

	size_t get_mark()
	{
		return _get;
	}

	void free_to_mark(size_t mark)
	{
		_put = mark;
	}
};

struct render_dx12_view_t
{
	ID3D12Resource* constant_buffer;
	render_view_data_t data;
};

struct render_dx12_target_t
{
	ID3D12Resource* resource;
	D3D12_CLEAR_VALUE clear_value;
	uint32_t name_hash;
};

struct render_dx12_pass_t
{
	size_t num_color_attachments;
	bool has_depth_stencil_attachment;

	uint32_t color_descriptor_offset[RENDER_MULTI_BUFFERING];
	D3D12_CPU_DESCRIPTOR_HANDLE color_descriptor[RENDER_MULTI_BUFFERING];

	uint32_t depth_stencil_descriptor_offset;
	D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_descriptor;

	size_t num_commands;
	render_command_t* commands;
};

struct render_dx12_script_t
{
	size_t num_targets;
	render_dx12_target_t* targets;

	size_t num_passes;
	render_dx12_pass_t* passes;
};

struct render_dx12_texture_t
{
	uint16_t width;
	uint16_t height;
	ID3D12Resource* resource;

	UINT64 upload_fence;
};

struct render_dx12_shader_property_t
{
	uint32_t name_hash;
	shader_frequency_t frequency;
	uint32_t pack_offset;
	uint32_t pack_size;
};

struct render_dx12_shader_variant_t
{
	ID3D12PipelineState* pipeline;
};

struct render_dx12_constant_buffer_pool_t
{
	size_t stride;
	uint8_t* default_data;
	ID3D12Resource* pool_buffer;
	idpool_t<uint16_t> pool;
};

struct render_dx12_shader_t
{
	render_dx12_constant_buffer_pool_t constant_buffers[SHADER_FREQUENCY_MAX];

	size_t num_properties;
	render_dx12_shader_property_t* properties;

	size_t num_variants;
	render_dx12_shader_variant_t* variants;
};

typedef enum render_dx12_property_type_t
{
	RENDER_PROPERTY_FLOAT_SCALAR = 0,
	RENDER_PROPERTY_INT_SCALAR,
	RENDER_PROPERTY_FLOAT_VEC4,
	RENDER_PROPERTY_FLOAT_MAT4,
	RENDER_PROPERTY_TEXTURE,
} render_dx12_property_type_t;

struct render_dx12_material_t
{
	render_dx12_shader_t* shader;

	uint16_t constant_buffer_ids[SHADER_FREQUENCY_MAX];
	uint8_t* constant_buffers[SHADER_FREQUENCY_MAX]; // CPU side

	UINT64 upload_fence;
};

struct render_dx12_mesh_t
{
	ID3D12Resource* vb;
	ID3D12Resource* ib;

	D3D12_VERTEX_BUFFER_VIEW vb_view;
	D3D12_INDEX_BUFFER_VIEW ib_view;

	size_t num_vertices;
	size_t num_indices;

	UINT64 upload_fence;
};

struct render_dx12_instance_t
{
	render_dx12_mesh_t* mesh;
	render_dx12_material_t* material;
	render_instance_data_t data;
};

struct render_dx12_upload_t
{
	ID3D12Resource* resoure;
	D3D12_RESOURCE_STATES src_state;
	D3D12_RESOURCE_STATES dst_state;
	UINT64 upload_fence;
};

struct render_dx12_t : public render_t
{
	allocator_t* allocator;

	objpool_t<render_dx12_view_t,      render_view_id_t>     views;
	objpool_t<render_dx12_script_t,    render_script_id_t>   scripts;
	objpool_t<render_dx12_texture_t,   render_texture_id_t>  textures;
	objpool_t<render_dx12_shader_t,    render_shader_id_t>   shaders;
	objpool_t<render_dx12_material_t,  render_material_id_t> materials;
	objpool_t<render_dx12_mesh_t,      render_mesh_id_t>     meshes;

	cobjpool_t<render_dx12_instance_t, render_instance_id_t> instances;

	array_t<render_dx12_upload_t> pending_uploads;

	IDXGIFactory4* dxgi_factory;
	ID3D12Device* device;
	
	ID3D12CommandQueue* queue;
	ID3D12CommandQueue* copy_queue;
	ID3D12Fence* fence;
	ID3D12Fence* copy_fence;
	HANDLE event;
	uint64_t frame_no;
	uint64_t copy_frame_no;

	struct frame_data_t
	{
		uint64_t fence_value;
		ID3D12CommandAllocator* allocator;
		ID3D12GraphicsCommandList* command_list;

		array_t<IUnknown*> delay_delete_queue;
		size_t upload_fence_mark;
	} frame[RENDER_MULTI_BUFFERING];
	
	struct copy_frame_data_t
	{
		uint64_t fence_value;
		ID3D12CommandAllocator* allocator;
		ID3D12GraphicsCommandList* command_list;

		size_t upload_fence_mark;
	} copy_frame[RENDER_MULTI_BUFFERING];

	ID3D12Resource* upload_resource;
	ID3D12Resource* copy_upload_resource;
	char* upload_buffer;
	char* copy_upload_buffer;
	ring_dx12_buffer_t upload_ring;
	ring_dx12_buffer_t copy_upload_ring;

	UINT32 rtv_size;
	UINT32 dsv_size;
	UINT32 cbv_srv_uav_size;
	UINT32 smp_size;

	range_pool_t<UINT32> rtv_pool;
	ID3D12DescriptorHeap* rtv_heap;

	idpool_t<UINT32> dsv_pool;
	ID3D12DescriptorHeap* dsv_heap;

	range_pool_t<UINT32> cbv_srv_uav_pool;
	ID3D12DescriptorHeap* cbv_srv_uav_heap;

	range_pool_t<UINT32> smp_pool;
	ID3D12DescriptorHeap* smp_heap;

	uint32_t width;
	uint32_t height;
	IDXGISwapChain3* swapchain;
	struct backbuffer_data_t
	{
		ID3D12Resource* resource;
	} backbuffer[RENDER_MULTI_BUFFERING];

	ID3D12Resource* argument_buffer;
	ID3D12Resource* instance_buffer;

	ID3D12RootSignature* root_signature;
	ID3D12CommandSignature* command_signature;
};

struct render_dx12_indirect_argument_t
{
	UINT32 start_index;
	D3D12_DRAW_INDEXED_ARGUMENTS draw_args;
};