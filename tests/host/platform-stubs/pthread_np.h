/* Host syntax checks only; never used by the PS5 build. */
#pragma once
#include <pthread.h>
extern "C" int pthread_attr_get_np(pthread_t, pthread_attr_t *);
