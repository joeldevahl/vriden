#pragma once

#include <stdarg.h>

#include "detect.h"

enum assert_action_t
{
	ASSERT_ACTION_NONE,
	ASSERT_ACTION_BREAK
};

typedef assert_action_t (*assert_callback_t)(const char* cond, const char* msg, const char* file, unsigned int line, void* user_data);

void assert_set_callback(assert_callback_t callback, void* user_data);

#ifdef ASSERT
#	undef ASSERT
#endif

#if defined(COMPILER_MSVC)
#	define BREAKPOINT() __debugbreak()
#elif defined(PLATFORM_OSX)
#	define BREAKPOINT() __builtin_trap()
#elif defined(COMPILER_GCC) && defined(ARCH_X86)
#	define BREAKPOINT() __asm__ ( "int %0" : :"I"(3) )
#elif defined(COMPILER_GCC) && defined(ARCH_PPC)
#	define BREAKPOINT() __asm__ volatile ("trap")
#else
#	error not defined for this platform
#endif

assert_action_t assert_call_trampoline(const char* file, unsigned int line, const char* cond, const char* fmt, ...);
assert_action_t assert_call_trampoline(const char* file, unsigned int line, const char* cond);

// TODO: replace with a real global assert handler and logging
#if defined(COMPILER_MSVC)
#	define ASSERT(cond, ...) ( (void)( ( !(cond) ) && ( assert_call_trampoline( __FILE__, __LINE__, #cond, __VA_ARGS__ ) == ASSERT_ACTION_BREAK ) && ( BREAKPOINT(), 1 ) ) )
#elif defined(COMPILER_GCC)
#	define ASSERT(cond, args...) ( (void)( ( !(cond) ) && ( assert_call_trampoline( __FILE__, __LINE__, #cond, ##args ) == ASSERT_ACTION_BREAK ) && ( BREAKPOINT(), 1 ) ) )
#else
#	error not defined for this platform
#endif

#define STATIC_ASSERT(cond) do { int static_assert_array[(cond) ? 1 : -1]; (void)static_assert_array[0]; } while(0)
