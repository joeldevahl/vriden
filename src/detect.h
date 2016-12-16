#pragma once

/**
 * Magic detection of platform, compiler and arch
 * Works on my machine
 */

#if defined(WIN64) || defined(_WIN64)
#	define FAMILY_WINDOWS
#	define PLATFORM_WIN64
#	define COMPILER_MSVC
#elif defined(WIN32) || defined(_WIN32)
#	define FAMILY_WINDOWS
#	define PLATFORM_WIN32
#	define COMPILER_MSVC
#endif

#if defined(__LINUX__) || defined(__linux__)
#	define FAMILY_UNIX
#	define PLATFORM_LINUX
#	define COMPILER_GCC
#endif

#if defined(MACOSX) || defined(__APPLE__) || defined(__DARWIN__)
#	define FAMILY_UNIX
#	define PLATFORM_OSX
#	define PLATFORM_OSX_PPC32 // only supported for now
#	define COMPILER_GCC
#endif

#if defined(i386) || defined(__i386__) || defined(__x86__) || defined(i386_AT386) || defined(PLATFORM_WIN32)
#	define ARCH_X86
#	define ARCH_X86_32
#endif

#if defined(__amd64__) || defined(__x86_64__)
#	define ARCH_X86
#	define ARCH_X86_64
#endif

#if defined(__powerpc__) || defined(__ppc__)
#	define ARCH_PPC
#	define ARCH_PPC_32 // only supported for now
#endif
