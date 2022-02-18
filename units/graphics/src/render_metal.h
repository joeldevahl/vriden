#pragma once

#include <graphics/render.h>
#include "render_common.h"

#include <foundation/objpool.h>
#include <foundation/cobjpool.h>

/* external types */
#include <units/graphics/types/mesh.h>
#include <units/graphics/types/texture.h>
#include <units/graphics/types/shader.h>
#include <units/graphics/types/texture.h>
#include <units/graphics/types/material.h>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

struct render_metal_view_t
{
	render_view_data_t data;
};

struct render_metal_script_t
{
};

struct render_metal_texture_t
{
	uint16_t width;
	uint16_t height;
};


struct render_metal_shader_t
{
};

struct render_metal_material_t
{
	render_metal_shader_t* shader;
};

struct render_metal_mesh_t
{
};

struct render_metal_instance_t
{
	render_metal_mesh_t* mesh;
	render_metal_material_t* material;
	render_instance_data_t data;
};

struct render_metal_t : public render_t
{
	allocator_t* allocator;

	objpool_t<render_metal_view_t,      render_view_id_t>     views;
	objpool_t<render_metal_script_t,    render_script_id_t>   scripts;
	objpool_t<render_metal_texture_t,   render_texture_id_t>  textures;
	objpool_t<render_metal_shader_t,    render_shader_id_t>   shaders;
	objpool_t<render_metal_material_t,  render_material_id_t> materials;
	objpool_t<render_metal_mesh_t,      render_mesh_id_t>     meshes;

	cobjpool_t<render_metal_instance_t, render_instance_id_t> instances;

	NSWindow* window;
	NSView* view;
	CAMetalLayer* layer;

	id<MTLDevice> device;
	id<MTLCommandQueue> command_queue;
};
