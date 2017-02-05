#pragma once

struct mutex_t;

mutex_t* mutex_create();

void mutex_destroy(mutex_t* mutex);

void mutex_lock(mutex_t* mutex);

void mutex_unlock(mutex_t* mutex);

struct scoped_lock
{
	mutex_t* _mutex;

	scoped_lock(mutex_t* mutex) : _mutex(mutex)
	{
		mutex_lock(_mutex);
	}

	~scoped_lock()
	{
		mutex_unlock(_mutex);
	}
};

#define SCOPED_LOCK(mutex) scoped_lock __scoped_lock__ ## __LINE__ (mutex)
