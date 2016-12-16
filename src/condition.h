#pragma once

#include "mutex.h"

struct condition_t;

condition_t* condition_create();

void condition_destroy(condition_t* condition);

void condition_wait(condition_t* condition, mutex_t* mutex);

void condition_signal(condition_t* condition);

void condition_broadcast(condition_t* condition);
