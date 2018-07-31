#pragma once

#include <graphics/render.h>
#include "render_common.h"

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

struct render_metal_t : public render_t
{
	allocator_t* allocator;

	NSWindow* window;
	NSView* view;
	CAMetalLayer* layer;

	id<MTLDevice> device;
	id<MTLCommandQueue> command_queue;
};
