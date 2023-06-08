#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"

#include <pthread.h>
#include <inttypes.h>

pthread_mutex_t buffer_lock = PTHREAD_MUTEX_INITIALIZER;

bool buffer_use(buffer_t *buf)
{
  if (!buf) {
    return false;
  }

  pthread_mutex_lock(&buffer_lock);
  if (buf->enqueued) {
    pthread_mutex_unlock(&buffer_lock);
    return false;
  }

  buf->mmap_reflinks += 1;
  pthread_mutex_unlock(&buffer_lock);
  return true;
}

bool buffer_consumed(buffer_t *buf, const char *who)
{
  if (!buf) {
    return false;
  }

  pthread_mutex_lock(&buffer_lock);
  if (buf->mmap_reflinks == 0) {
    LOG_PERROR(buf, "Non symmetric reference counts");
  }

  buf->mmap_reflinks--;

  if (!buf->enqueued && buf->mmap_reflinks == 0) {
    LOG_DEBUG(buf, "Queuing buffer... used=%zu length=%zu (linked=%s) by %s",
      buf->used,
      buf->length,
      buf->dma_source ? buf->dma_source->name : NULL,
      who);

    // Assign or clone timestamp
    if (buf->buf_list->do_timestamps) {
      buf->captured_time_us = get_monotonic_time_us(NULL, NULL);
    }

    if (buf->buf_list->dev->hw->buffer_enqueue(buf, who) < 0) {
      goto error;
    }

    buf->enqueued = true;
    buf->enqueue_time_us = buf->buf_list->last_enqueued_us = get_monotonic_time_us(NULL, NULL);
  }

  pthread_mutex_unlock(&buffer_lock);
  return true;

error:
  {
    buffer_t *dma_source = buf->dma_source;
    buf->dma_source = NULL;
    buf->mmap_reflinks++;
    pthread_mutex_unlock(&buffer_lock);

    if (dma_source) {
      buffer_consumed(dma_source, who);
    }
  }

  return false;
}

buffer_t *buffer_list_find_slot(buffer_list_t *buf_list)
{
  buffer_t *buf = NULL;

  for (int i = 0; i < buf_list->nbufs; i++) {
    if (!buf_list->bufs[i]->enqueued && buf_list->bufs[i]->mmap_reflinks == 1) {
      buf = buf_list->bufs[i];
      break;
    }
  }

  return buf;
}

int buffer_list_count_enqueued(buffer_list_t *buf_list)
{
  int n = 0;

  for (int i = 0; i < buf_list->nbufs; i++) {
    if (buf_list->bufs[i]->enqueued) {
      n++;
    }
  }

  return n;
}

int buffer_list_enqueue(buffer_list_t *buf_list, buffer_t *dma_buf)
{
  if (!buf_list->do_mmap && !dma_buf->buf_list->do_mmap) {
    LOG_PERROR(buf_list, "Cannot enqueue non-mmap to non-mmap: %s.", dma_buf->name);
  }

  buffer_t *buf = buffer_list_find_slot(buf_list);
  if (!buf) {
    return 0;
  }

  buf->flags = dma_buf->flags;
  buf->captured_time_us = dma_buf->captured_time_us;

  if (buf_list->do_mmap) {
    if (dma_buf->used > buf->length) {
      LOG_INFO(buf_list, "The dma_buf (%s) is too long: %zu vs space=%zu",
        dma_buf->name, dma_buf->used, buf->length);
      dma_buf->used = buf->length;
    }

    uint64_t before = get_monotonic_time_us(NULL, NULL);
    memcpy(buf->start, dma_buf->start, dma_buf->used);
    uint64_t after = get_monotonic_time_us(NULL, NULL);

    LOG_DEBUG(buf, "mmap copy: dest=%p, src=%p (%s), size=%zu, space=%zu, time=%" PRIu64 "us",
      buf->start, dma_buf->start, dma_buf->name, dma_buf->used, buf->length, after-before);
  } else {
    LOG_DEBUG(buf, "dmabuf copy: dest=%p, src=%p (%s, dma_fd=%d), size=%zu",
      buf->start, dma_buf->start, dma_buf->name, dma_buf->dma_fd, dma_buf->used);

    buf->dma_source = dma_buf;
    buf->length = dma_buf->length;
    dma_buf->mmap_reflinks++;
  }

  buf->used = dma_buf->used;
  buffer_consumed(buf, "copy-data");
  return 1;
}

static void buffer_update_h264_key_frame(buffer_t *buf)
{
  unsigned char *data = buf->start;

  static const int N = 8;
  char buffer [3*N+1];
  buffer[sizeof(buffer)-1] = 0;
  for(int j = 0; j < N; j++)
    sprintf(&buffer[sizeof(buffer)/N*j], "%02X ", data[j]);

  if (buf->flags.is_keyframe) {
    LOG_DEBUG(buf, "Got key frame (from V4L2)!: %s", buffer);
  } else if (buf->used >= 5 && (data[4] & 0x1F) == 0x07) {
    LOG_DEBUG(buf, "Got key frame (from buffer)!: %s", buffer);
    buf->flags.is_keyframe = true;
  }
}

buffer_t *buffer_list_dequeue(buffer_list_t *buf_list)
{
  buffer_t *buf = NULL;

  if (buf_list->dev->hw->buffer_list_dequeue(buf_list, &buf) < 0) {
    goto error;
  }

  uint64_t dequeued_us = 0;
  if (buf_list->last_dequeued_us > 0)
    dequeued_us = get_monotonic_time_us(NULL, NULL) - buf_list->last_dequeued_us;

  buf_list->last_dequeued_us = get_monotonic_time_us(NULL, NULL);
  buf_list->last_capture_time_us = buf_list->last_dequeued_us - buf->captured_time_us;
  buf_list->last_in_queue_time_us = buf_list->last_dequeued_us - buf->enqueue_time_us;

  if (buf->mmap_reflinks > 0) {
    LOG_PERROR(buf, "Buffer appears to be enqueued? (links=%d)", buf->mmap_reflinks);
  }

  buf->enqueued = false;
  buf->mmap_reflinks = 1;

	LOG_DEBUG(buf_list, "Grabbed mmap buffer=%u, bytes=%zu, used=%zu, frame=%d, linked=%s",
    buf->index,
    buf->length,
    buf->used,
    buf_list->stats.frames,
    buf->dma_source ? buf->dma_source->name : NULL);

  if (buf->dma_source) {
    buf->dma_source->used = 0;
    buffer_consumed(buf->dma_source, "mmap-dequeued");
    buf->dma_source = NULL;
  }

  if (buf_list->fmt.format == V4L2_PIX_FMT_H264) {
    buffer_update_h264_key_frame(buf);
    buf->flags.is_keyed = true;
  } else {
    buf->flags.is_keyed = false;
  }

  buf_list->stats.frames++;

  float old_average = buf_list->stats.avg_dequeued_us;
  float old_sum = buf_list->stats.avg_dequeued_us * buf_list->stats.frames_since_reset;
  float old_stddev_sum = buf_list->stats.stddev_dequeued_us * buf_list->stats.stddev_dequeued_us * buf_list->stats.frames_since_reset;

  buf_list->stats.frames_since_reset++;
  if (dequeued_us > buf_list->stats.max_dequeued_us)
    buf_list->stats.max_dequeued_us = dequeued_us;
  buf_list->stats.avg_dequeued_us = (float)(old_sum + dequeued_us) / buf_list->stats.frames_since_reset;
  buf_list->stats.stddev_dequeued_us = sqrt(
    ( old_stddev_sum + (dequeued_us - old_average) * (dequeued_us - buf_list->stats.avg_dequeued_us)
    ) / buf_list->stats.frames_since_reset);
  return buf;

error:
  return NULL;
}

int buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue)
{
  return buf_list->dev->hw->buffer_list_pollfd(buf_list, pollfd, can_dequeue);
}

void buffer_list_clear_queue(buffer_list_t *buf_list)
{
  ARRAY_FOREACH(buffer_t*, queued_buf, buf_list->queued_bufs, buf_list->n_queued_bufs) {
    buffer_consumed(*queued_buf, "clear queue");
    *queued_buf = NULL;
  }
  buf_list->n_queued_bufs = 0;
}

bool buffer_list_push_to_queue(buffer_list_t *buf_list, buffer_t *dma_buf, int max_bufs)
{
  max_bufs = MIN(max_bufs ? max_bufs : MAX_BUFFER_QUEUE, MAX_BUFFER_QUEUE);

  if (buf_list->dev->paused)
    return true;
  if (buf_list->n_queued_bufs >= max_bufs)
    return false;

  buffer_use(dma_buf);
  buf_list->queued_bufs[buf_list->n_queued_bufs++] = dma_buf;
  return true;
}

buffer_t *buffer_list_pop_from_queue(buffer_list_t *buf_list)
{
  if (buf_list->n_queued_bufs <= 0)
    return NULL;

  buffer_t *buf = buf_list->queued_bufs[0];
  buf_list->n_queued_bufs--;

  for (int i = 0; i < buf_list->n_queued_bufs; i++) {
    buf_list->queued_bufs[i] = buf_list->queued_bufs[i+1];
  }

  return buf;
}
