#include <stdio.h>
#include <stdarg.h>

#if defined(FAMILY_WINDOWS)
#	include <windows.h> // TODO: include own windows headers with defines
#endif

#include "assert.h"
#include "defines.h"

static assert_callback_t g_assert_callback = 0x0;
static void* g_assert_callback_data = 0x0;

void assert_set_callback( assert_callback_t callback, void* user_data )
{
	g_assert_callback = callback;
	g_assert_callback_data = user_data;
}

assert_action_t assert_call_trampoline(const char* file, unsigned int line, const char* cond)
{
	if( g_assert_callback != 0x0 )
		return g_assert_callback(cond, "", file, line, g_assert_callback_data);
	return ASSERT_ACTION_BREAK;
}

assert_action_t assert_call_trampoline(const char* file, unsigned int line, const char* cond, const char* fmt, ...)
{

	char buffer[2048];
	va_list list;
	va_start( list, fmt );
	vsnprintf(buffer, 2048, fmt, list);
	va_end(list);
	buffer[2048 - 1] = 0;
#ifdef FAMILY_WINDOWS
	OutputDebugStringA(buffer);
	OutputDebugStringA("\n");
#endif
	fputs(buffer, stderr);
	fputs("\n", stderr);
	if( g_assert_callback == 0x0 )
		return ASSERT_ACTION_BREAK;
	return g_assert_callback( cond, buffer, file, line, g_assert_callback_data );
}
