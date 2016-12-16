#pragma once

#include "mutex.h"

#include <cstdlib>

#ifdef FAMILY_WINDOWS
#	include <windows.h> // TODO: use a common windows header with some defines in front!
#elif defined(FAMILY_UNIX)
#	include <pthread.h>
#else
#	error not implemented for this platform
#endif

struct mutex_t
{
#ifdef FAMILY_WINDOWS
	CRITICAL_SECTION mutex;
#elif defined(FAMILY_UNIX)
	pthread_mutex_t mutex;
#else
#	error not implemented for this platform
#endif
};
