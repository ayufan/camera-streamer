#pragma once

#include "buffer.h"

#include <pthread.h>

typedef struct buffer_lock_s {
  const char *name;
  pthread_mutex_t lock;
  pthread_cond_t cond_wait;
  buffer_t *buf;
  int counter;
} buffer_lock_t;

#define DEFINE_BUFFER_LOCK(name) static buffer_lock_t name = { #name, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, NULL, 0 };

void buffer_lock_capture(buffer_lock_t *buf_lock, buffer_t *buf);
buffer_t *buffer_lock_get(buffer_lock_t *buf_lock, int timeout_s, int *counter);
