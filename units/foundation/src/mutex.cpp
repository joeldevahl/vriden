#include "assert.h"
#include "mutex_internal.h"

mutex_t* mutex_create()
{
	mutex_t* mutex = (mutex_t*)malloc(sizeof(mutex_t)); // TODO: use allocator? Store differently?
#if defined(FAMILY_WINDOWS)
	InitializeCriticalSection(&mutex->mutex);
#elif defined(FAMILY_UNIX)
	pthread_mutex_init(&mutex->mutex, NULL);
#else
#	error not implemented for this platform
#endif
	return mutex;
}

void mutex_destroy(mutex_t* mutex)
{
#if defined(FAMILY_WINDOWS)
	DeleteCriticalSection(&mutex->mutex);
#elif defined(FAMILY_UNIX)
	pthread_mutex_destroy(&mutex->mutex);
#else
#	error not implemented for this platform
#endif
	free(mutex);
}

void mutex_lock(mutex_t* mutex)
{
#if defined(FAMILY_WINDOWS)
	EnterCriticalSection(&mutex->mutex);
#elif defined(FAMILY_UNIX)
	pthread_mutex_lock(&mutex->mutex);
#else
#	error not implemented for this platform
#endif
}

void mutex_unlock(mutex_t* mutex)
{
#if defined(FAMILY_WINDOWS)
	LeaveCriticalSection(&mutex->mutex);
#elif defined(FAMILY_UNIX)
	pthread_mutex_unlock(&mutex->mutex);
#else
#	error not implemented for this platform
#endif
}
