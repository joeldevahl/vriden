#import <Cocoa/Cocoa.h>
#import <QuartzCore/CVDisplayLink.h>

#include <foundation/allocator.h>
#include <application/application.h>
#include <application/window.h>

@interface AtecApplication : NSApplication
@end

@implementation AtecApplication

- (void)sendEvent:(NSEvent *)event
{
	// TODO(joel): send events to window
	[super sendEvent:event];
}

@end

struct application_t
{
	allocator_t* allocator;
	int argc;
	const char** argv;
	NSAutoreleasePool* pool;
};

application_t* application_create(application_create_params_t* params)
{
	allocator_t* allocator = params->allocator;
	application_t* application = ALLOCATOR_ALLOC_TYPE(allocator, application_t);
	application->allocator = allocator;
	application->argc = params->argc;
	application->argv = params->argv;

	application->pool = [[NSAutoreleasePool alloc] init];

	[AtecApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

	id menubar = [[NSMenu new] autorelease];
    id appMenuItem = [[NSMenuItem new] autorelease];
    [menubar addItem:appMenuItem];
    [NSApp setMainMenu:menubar];
    id appMenu = [[NSMenu new] autorelease];
    id appName = [[NSProcessInfo processInfo] processName];
    id quitTitle = [@"Quit " stringByAppendingString:appName];
    id quitMenuItem = [[[NSMenuItem alloc] initWithTitle:quitTitle
        action:@selector(terminate:) keyEquivalent:@"q"] autorelease];
    [appMenu addItem:quitMenuItem];
    [appMenuItem setSubmenu:appMenu];
    [NSApp activateIgnoringOtherApps:YES];
	[NSApp finishLaunching];

	return application;
}

void application_destroy(application_t* application)
{
	allocator_t* allocator = application->allocator;
	[application->pool release];
	ALLOCATOR_FREE(allocator, application);
}

void application_update(application_t* application)
{
	NSEvent *event;

	do
	{
		event = [NSApp nextEventMatchingMask:-1 // TODO(joel): what event mask?
			untilDate:[NSDate distantPast]
			inMode:NSDefaultRunLoopMode
			dequeue:YES];

		if (event)
		{
			[NSApp sendEvent:event];
		}
	}
	while (event);
}

int application_get_argc(application_t* app)
{
	return app->argc;
}

const char** application_get_argv(application_t* app)
{
	return app->argv;
}

bool application_is_running(application_t* app)
{
	return true; // TODO(joel): keep track of window running
}
