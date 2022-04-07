#include "hw/buffer_lock.h"

bool buffer_lock_is_used(buffer_lock_t *buf_lock)
{
  int refs = 0;

  pthread_mutex_lock(&buf_lock->lock);
  refs = buf_lock->refs;
  pthread_mutex_unlock(&buf_lock->lock);

  return refs;
}

void buffer_lock_use(buffer_lock_t *buf_lock, int ref)
{
  pthread_mutex_lock(&buf_lock->lock);
  buf_lock->refs += ref;
  pthread_mutex_unlock(&buf_lock->lock);
}

bool buffer_lock_needs_buffer(buffer_lock_t *buf_lock)
{
  uint64_t now = get_monotonic_time_us(NULL, NULL);
  bool needs_buffer = false;

  pthread_mutex_lock(&buf_lock->lock);
  if (buf_lock->timeout_us > 0 && now - buf_lock->buf_time_us > buf_lock->timeout_us) {
    buffer_consumed(buf_lock->buf, buf_lock->name);
    buf_lock->buf = NULL;
  }
  if (buf_lock->refs > 0) {
    needs_buffer = true;
  }
  pthread_mutex_unlock(&buf_lock->lock);

  return needs_buffer;
}

void buffer_lock_capture(buffer_lock_t *buf_lock, buffer_t *buf)
{
  pthread_mutex_lock(&buf_lock->lock);
  buffer_consumed(buf_lock->buf, buf_lock->name);
  buffer_use(buf);
  buf_lock->buf = buf;
  buf_lock->counter++;
  buf_lock->buf_time_us = get_monotonic_time_us(NULL, NULL);
  uint64_t captured_time_s = get_time_us(CLOCK_FROM_PARAMS, NULL, &buf->v4l2_buffer.timestamp, 0);
  E_LOG_DEBUG(buf_lock, "Captured buffer %s (refs=%d), frame=%d, delay=%llus",
    dev_name(buf), buf ? buf->mmap_reflinks : 0, buf_lock->counter,
    buf_lock->buf_time_us - captured_time_s);
  pthread_cond_broadcast(&buf_lock->cond_wait);
  pthread_mutex_unlock(&buf_lock->lock);
}

buffer_t *buffer_lock_get(buffer_lock_t *buf_lock, int timeout_ms, int *counter)
{
  buffer_t *buf = NULL;
  struct timespec timeout;

  if(!timeout_ms)
    timeout_ms = DEFAULT_BUFFER_LOCK_GET_TIMEOUT;

  get_time_us(CLOCK_REALTIME, &timeout, NULL, timeout_ms * 1000LL);

  pthread_mutex_lock(&buf_lock->lock);
  if (*counter == buf_lock->counter || !buf_lock->buf) {
    int ret = pthread_cond_timedwait(&buf_lock->cond_wait, &buf_lock->lock, &timeout);
    if (ret == ETIMEDOUT) {
      goto ret;
    } else if (ret < 0) {
      goto ret;
    }
  }

  buf = buf_lock->buf;
  buffer_use(buf);
  *counter = buf_lock->counter;

ret:
  pthread_mutex_unlock(&buf_lock->lock);
  return buf;
}

int buffer_lock_write_loop(buffer_lock_t *buf_lock, int nframes, buffer_write_fn fn, void *data)
{
  int counter = 0;
  int frames = 0;

  buffer_lock_use(buf_lock, 1);

  while (nframes == 0 || frames < nframes) {
    buffer_t *buf = buffer_lock_get(buf_lock, 0, &counter);
    if (!buf) {
      goto error;
    }

    int ret = fn(buf_lock, buf, frames, data);
    buffer_consumed(buf, "write-loop");

    if (ret > 0) {
      frames++;
    } else if (ret < 0) {
      goto error;
    }
  }

ok:
  buffer_lock_use(buf_lock, -1);
  return frames;

error:
  buffer_lock_use(buf_lock, -1);
  return -frames;
}
