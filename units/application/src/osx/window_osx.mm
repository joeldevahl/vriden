#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include <foundation/allocator.h>
#include <application/application.h>
#include <application/window.h>

#define METALVIEW_TAG 255

@interface WindowDelegate : NSObject
@end

@implementation WindowDelegate
@end


@interface ContentView : NSView {
	NSInteger _tag;
}

- (instancetype)initWithFrame:(NSRect)frame scale:(CGFloat)scale;

@property (assign, readonly) NSInteger tag;

@end

@implementation ContentView

@synthesize tag = _tag;

+ (Class)layerClass
{
	return NSClassFromString(@"CAMetalLayer");
}

- (BOOL)wantsUpdateLayer
{
	return YES;
}

- (CALayer*)makeBackingLayer
{
	return [self.class.layerClass layer];
}

- (instancetype)initWithFrame:(NSRect)frame scale:(CGFloat)scale
{
	if ((self = [super initWithFrame:frame]))
	{
		_tag = METALVIEW_TAG;
		self.wantsLayer = YES;

		self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
		self.layer.contentsScale = scale;
	}

	return self;
}

- (void)resizeWithOldSuperviewSize:(NSSize)oldSize
{
	[super resizeWithOldSuperviewSize:oldSize];
}

@end

struct window_t
{
	allocator_t* allocator;
	id delegate_id;
	NSWindow* window_id;
	NSView* view_id;
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

	CGFloat scale = 1.0f;
	if ([window->window_id.screen respondsToSelector:@selector(backingScaleFactor)])
		scale = window->window_id.screen.backingScaleFactor;
	
	window->view_id = [[ContentView alloc] initWithFrame: window->window_id.frame scale:scale];
	[window->view_id setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
	[window->view_id setNeedsDisplay:YES];
	[window->window_id setContentView:window->view_id];
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
