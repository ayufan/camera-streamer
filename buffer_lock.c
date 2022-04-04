#include "buffer_lock.h"

void buffer_lock_capture(buffer_lock_t *buf_lock, buffer_t *buf)
{
  pthread_mutex_lock(&buf_lock->lock);
  buffer_consumed(buf_lock->buf);
  buffer_use(buf);
  buf_lock->buf = buf;
  buf_lock->counter++;
  E_LOG_DEBUG(buf_lock, "Captured HTTP snapshot: %d", buf_lock->counter);
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
  if (*counter == buf_lock->counter && timeout_s > 0) {
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
