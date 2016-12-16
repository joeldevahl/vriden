#pragma once

#include "detect.h"

#define BIT(b) (1<<b)
#define ARRAY_LENGTH(a) (sizeof((a)[0]) ? sizeof(a) / sizeof((a)[0]) : 0)
#define ALIGN_UP(ptr, align) (((uintptr_t)(ptr) + ((align)-1)) & ~((align)-1))

#if defined(COMPILER_GCC)
#	if defined(ALIGN)
#		undef ALIGN
#	endif
#	define ALIGN(a) __attribute__ ((aligned (a)))
#elif defined(COMPILER_MSVC)
#	define ALIGN(a) __declspec(align(a))
#else
#	error ALIGN not defined for this compiler
#endif

#if defined(COMPILER_GCC) || defined(COMPILER_MSVC)
#	define ALIGNOF __alignof
#else
#	error ALIGNOF not defined for this compiler
#endif

#if defined(COMPILER_GCC)
#	define FORCE_INLINE static inline __attribute__((always_inline))
#elif defined(COMPILER_MSVC)
#	define FORCE_INLINE __inline
#else
#	error FORCE_INLINE not defined for this compiler
#endif

#if defined(COMPILER_GCC)
#	define RESTRICT __restrict__
#elif defined(COMPILER_MSVC)
#	define RESTRICT __restrict
#else
#	error RESTRICT not defined for this compiler
#endif
