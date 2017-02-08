#import <Cocoa/Cocoa.h>

#include <foundation/allocator.h>
#include <application/window.h>

@interface WindowDelegate : NSObject
@end

@implementation WindowDelegate
@end

@interface ContentView : NSView
@end

@implementation ContentView

- (BOOL)isOpaque
{
    return YES;
}

- (BOOL)canBecomeKeyView
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

@end

struct window_t
{
	allocator_t* allocator;
	id delegate_id;
	id window_id;
};

window_t* window_create(window_create_params_t* params)
{
	allocator_t* allocator = params->allocator;
	window_t* window = ALLOCATOR_ALLOC_TYPE(allocator, window_t);
	window->allocator = allocator;

	window->delegate_id = [[WindowDelegate alloc] init];
	[NSApp setDelegate:window->delegate_id];

	unsigned int window_style_mask = 0; // TODO(joel): masks where deprecated?
	window->window_id = [[NSWindow alloc]
		initWithContentRect:NSMakeRect(0, 0, 1280, 720)
				  styleMask:window_style_mask
					backing:NSBackingStoreBuffered
					  defer:NO];
    [window->window_id setContentView:[[ContentView alloc] init]];
	[window->window_id setDelegate:window->delegate_id];
	[window->window_id setAcceptsMouseMovedEvents:YES];
	[window->window_id center];
	[window->window_id makeKeyAndOrderFront:nil];

	return window;
}

void window_destroy(window_t* window)
{
	allocator_t* allocator = window->allocator;

	[window->window_id setDelegate:nil];
	[NSApp setDelegate:nil];
	[window->delegate_id release];

	[window->window_id close];

	ALLOCATOR_FREE(allocator, window);
}

void* window_get_platform_handle(window_t* window)
{
	return (void*)window->window_id;
}
