#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>

#include "allocator.h"
#include "window.h"

struct window_t
{
	allocator_t* allocator;
	HWND hwnd;
};

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch(message)
	{
		case WM_SIZE:
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_INPUT:
			break;
	}

	return DefWindowProc(hwnd, message, wparam, lparam);
}

window_t* window_create(window_create_params_t* params)
{

	const PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		0,
		PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW,
		PFD_TYPE_RGBA,
		32,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		24,
		8,
		0,
		PFD_MAIN_PLANE,
		0,
		0,
		0,
		0,
	};

	WNDCLASS wc;
	HINSTANCE hinstance;

	window_t* window = ALLOCATOR_ALLOC_TYPE(params->allocator, window_t);
	window->allocator = params->allocator;

	hinstance = GetModuleHandle(NULL);

	const char* name = "vriden"; // TODO: name from creation params here
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = wnd_proc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hinstance;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = name;

	RegisterClass(&wc);

	RECT wr = { 0, 0, 1280, 720 };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

	window->hwnd = CreateWindowEx(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, name, NULL, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, hinstance, NULL);

	ShowWindow(window->hwnd, SW_SHOW);
	UpdateWindow(window->hwnd);

	return window;
}

void window_destroy(window_t* window)
{
	ALLOCATOR_FREE(window->allocator, window);
}

void* window_get_platform_handle(window_t* window)
{
	return (void*)window->hwnd;
}
