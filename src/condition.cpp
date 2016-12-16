#include "assert.h"
#include "condition.h"
#include "mutex_internal.h"

#include <stdint.h>
#include <cstdlib>

#ifdef FAMILY_WINDOWS
#	include <windows.h> // TODO: use a common windows header with some defines in front!
#elif defined(FAMILY_UNIX)
#	include <pthread.h>
#else
#	error not implemented for this platform
#endif

struct condition_t
{
#ifdef FAMILY_WINDOWS
	CONDITION_VARIABLE condition;
#elif defined(FAMILY_UNIX)
	pthread_cond_t condition;
#else
#	error not implemented for this platform
#endif
};

condition_t* condition_create()
{
	condition_t* condition = (condition_t*)malloc(sizeof(condition_t)); // TODO: use allocator? Store differently
#if defined(FAMILY_WINDOWS)
	InitializeConditionVariable(&condition->condition);
#elif defined(FAMILY_UNIX)
	pthread_cond_init(&condition->condition, NULL);
#else
#	error not implemented for this platform
#endif
	return condition;
}

void condition_destroy(condition_t* condition)
{
#if defined(FAMILY_WINDOWS)

	// Conditions are not deleted on windows
#elif defined(FAMILY_UNIX)
	pthread_cond_destroy(&condition->condition);
#else
#	error not implemented for this platform
#endif
	free(condition);
}

void condition_wait(condition_t* condition, mutex_t* mutex)
{
#if defined(FAMILY_WINDOWS)
		SleepConditionVariableCS(&condition->condition, &mutex->mutex, INFINITE);
#elif defined(FAMILY_UNIX)
		pthread_cond_wait(&condition->condition, &mutex->mutex);
#else
#	error not implemented for this platform
#endif
}

void condition_signal(condition_t* condition)
{
#if defined(FAMILY_WINDOWS)
	WakeConditionVariable(&condition->condition);
#elif defined(FAMILY_UNIX)
	pthread_cond_signal(&condition->condition);
#else
#	error not implemented for this platform
#endif
}

void condition_broadcast(condition_t* condition)
{
#if defined(FAMILY_WINDOWS)
	WakeAllConditionVariable(&condition->condition);
#elif defined(FAMILY_UNIX)
	pthread_cond_broadcast(&condition->condition);
#else
#	error not implemented for this platform
#endif
}
