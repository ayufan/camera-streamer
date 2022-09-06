#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

typedef struct buffer_s buffer_t;
typedef struct buffer_list_s buffer_list_t;
typedef struct buffer_lock_s buffer_lock_t;

typedef bool (*buffer_lock_check_streaming)(buffer_lock_t *buf_lock);
typedef void (*buffer_lock_notify_buffer)(buffer_lock_t *buf_lock, buffer_t *buf);

#define BUFFER_LOCK_MAX_CALLBACKS 10

typedef struct buffer_lock_s {
  const char *name;
  buffer_list_t *buf_list;

  buffer_lock_check_streaming check_streaming[BUFFER_LOCK_MAX_CALLBACKS];
  buffer_lock_notify_buffer notify_buffer[BUFFER_LOCK_MAX_CALLBACKS];

  // private
  pthread_mutex_t lock;
  pthread_cond_t cond_wait;
  buffer_t *buf;
  uint64_t buf_time_us;
  int counter;
  int refs;
  int dropped;
  uint64_t timeout_us;

  int frame_interval_ms;
} buffer_lock_t;

#define DEFAULT_BUFFER_LOCK_TIMEOUT 16 // ~60fps
#define DEFAULT_BUFFER_LOCK_GET_TIMEOUT 2000 // 2s

#define DEFINE_BUFFER_LOCK(_name, _timeout_ms) buffer_lock_t _name = { \
    .name = #_name, \
    .lock = PTHREAD_MUTEX_INITIALIZER, \
    .cond_wait = PTHREAD_COND_INITIALIZER, \
    .timeout_us = (_timeout_ms > DEFAULT_BUFFER_LOCK_TIMEOUT ? _timeout_ms : DEFAULT_BUFFER_LOCK_TIMEOUT) * 1000LL, \
  };

#define DECLARE_BUFFER_LOCK(_name) extern buffer_lock_t _name;

typedef int (*buffer_write_fn)(buffer_lock_t *buf_lock, buffer_t *buf, int frame, void *data);

void buffer_lock_capture(buffer_lock_t *buf_lock, buffer_t *buf);
buffer_t *buffer_lock_get(buffer_lock_t *buf_lock, int timeout_ms, int *counter);
bool buffer_lock_needs_buffer(buffer_lock_t *buf_lock);
void buffer_lock_use(buffer_lock_t *buf_lock, int ref);
bool buffer_lock_is_used(buffer_lock_t *buf_lock);
int buffer_lock_write_loop(buffer_lock_t *buf_lock, int nframes, unsigned timeout_ms, buffer_write_fn fn, void *data);
bool buffer_lock_register_check_streaming(buffer_lock_t *buf_lock, buffer_lock_check_streaming check_streaming);
bool buffer_lock_register_notify_buffer(buffer_lock_t *buf_lock, buffer_lock_notify_buffer notify_buffer);
