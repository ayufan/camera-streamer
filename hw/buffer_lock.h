#pragma once

#include "buffer.h"

#include <pthread.h>

typedef struct buffer_lock_s {
  const char *name;
  pthread_mutex_t lock;
  pthread_cond_t cond_wait;
  buffer_t *buf;
  uint64_t buf_time_us;
  int counter;
  int refs;
  uint64_t timeout_us;
} buffer_lock_t;

#define DEFAULT_BUFFER_LOCK_TIMEOUT 16 // ~60fps
#define DEFAULT_BUFFER_LOCK_GET_TIMEOUT 2000 // 2s

#define DEFINE_BUFFER_LOCK(_name, _timeout_ms) static buffer_lock_t _name = { \
    .name = #_name, \
    .lock = PTHREAD_MUTEX_INITIALIZER, \
    .cond_wait = PTHREAD_COND_INITIALIZER, \
    .timeout_us = (_timeout_ms > DEFAULT_BUFFER_LOCK_TIMEOUT ? _timeout_ms : DEFAULT_BUFFER_LOCK_TIMEOUT) * 1000LL, \
  };

typedef int (*buffer_write_fn)(buffer_lock_t *buf_lock, buffer_t *buf, int frame, void *data);

void buffer_lock_capture(buffer_lock_t *buf_lock, buffer_t *buf);
buffer_t *buffer_lock_get(buffer_lock_t *buf_lock, int timeout_ms, int *counter);
bool buffer_lock_needs_buffer(buffer_lock_t *buf_lock);
void buffer_lock_use(buffer_lock_t *buf_lock, int ref);
bool buffer_lock_is_used(buffer_lock_t *buf_lock);
int buffer_lock_write_loop(buffer_lock_t *buf_lock, int nframes, buffer_write_fn fn, void *data);
