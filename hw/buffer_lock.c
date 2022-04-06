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
  struct timeval now;
  gettimeofday(&now, NULL);
  bool needs_buffer = false;

  pthread_mutex_lock(&buf_lock->lock);
  if (now.tv_sec - buf_lock->buf_time.tv_sec > 1) {
    buffer_consumed(buf_lock->buf, buf_lock->name);
    buf_lock->buf = NULL;
    needs_buffer = true;
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
  gettimeofday(&buf_lock->buf_time, NULL);
  E_LOG_DEBUG(buf_lock, "Captured buffer %s (refs=%d), frame=%d", dev_name(buf), buf ? buf->mmap_reflinks : 0, buf_lock->counter);
  pthread_cond_broadcast(&buf_lock->cond_wait);
  pthread_mutex_unlock(&buf_lock->lock);
}

buffer_t *buffer_lock_get(buffer_lock_t *buf_lock, int timeout_s, int *counter)
{
  buffer_t *buf = NULL;
  struct timeval now;
  struct timespec timeout;
  gettimeofday(&now, NULL);
  timeout.tv_nsec = now.tv_usec;
  timeout.tv_sec = now.tv_sec + timeout_s;

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
