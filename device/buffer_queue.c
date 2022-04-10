#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/hw/v4l2.h"

#include <pthread.h>

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
    E_LOG_PERROR(buf, "Non symmetric reference counts");
  }

  buf->mmap_reflinks--;

  if (!buf->enqueued && buf->mmap_reflinks == 0) {
    if (buf->buf_list->device->hw->buffer_enqueue(buf, who) < 0) {
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
    E_LOG_PERROR(buf_list, "Cannot enqueue non-mmap to non-mmap: %s.", dma_buf->name);
  }

  buffer_t *buf = buffer_list_find_slot(buf_list);
  if (!buf) {
    return 0;
  }

  buf->v4l2.flags = 0;
  buf->v4l2.flags &= ~V4L2_BUF_FLAG_KEYFRAME;
  buf->v4l2.flags |= dma_buf->v4l2.flags & V4L2_BUF_FLAG_KEYFRAME;
  buf->captured_time_us = dma_buf->captured_time_us;

  if (buf_list->do_mmap) {
    if (dma_buf->used > buf->length) {
      E_LOG_INFO(buf_list, "The dma_buf (%s) is too long: %zu vs space=%zu",
        dma_buf->name, dma_buf->used, buf->length);
      dma_buf->used = buf->length;
    }

    uint64_t before = get_monotonic_time_us(NULL, NULL);
    memcpy(buf->start, dma_buf->start, dma_buf->used);
    uint64_t after = get_monotonic_time_us(NULL, NULL);

    E_LOG_DEBUG(buf, "mmap copy: dest=%p, src=%p (%s), size=%zu, space=%zu, time=%dllus",
      buf->start, dma_buf->start, dma_buf->name, dma_buf->used, buf->length, after-before);
  } else {
    E_LOG_DEBUG(buf, "dmabuf copy: dest=%p, src=%p (%s, dma_fd=%d), size=%zu",
      buf->start, dma_buf->start, dma_buf->name, dma_buf->dma_fd, dma_buf->used);

    buf->dma_source = dma_buf;
    buf->length = dma_buf->length;
    dma_buf->mmap_reflinks++;
  }

  buf->used = dma_buf->used;
  buffer_consumed(buf, "copy-data");
  return 1;
error:
  return -1;
}

buffer_t *buffer_list_dequeue(buffer_list_t *buf_list)
{
  buffer_t *buf = NULL;

  if (buf_list->device->hw->buffer_list_dequeue(buf_list, &buf) < 0) {
    goto error;
  }

  buf_list->last_dequeued_us = get_monotonic_time_us(NULL, NULL);

  if (buf->mmap_reflinks > 0) {
    E_LOG_PERROR(buf, "Buffer appears to be enqueued? (links=%d)", buf->mmap_reflinks);
  }

  buf->enqueued = false;
  buf->mmap_reflinks = 1;

	E_LOG_DEBUG(buf_list, "Grabbed mmap buffer=%u, bytes=%d, used=%d, frame=%d, linked=%s, flags=%08x",
    buf->index,
    buf->length,
    buf->used,
    buf_list->frames,
    buf->dma_source ? buf->dma_source->name : NULL,
    buf->v4l2.flags);

  if (buf->dma_source) {
    buf->dma_source->used = 0;
    buffer_consumed(buf->dma_source, "mmap-dequeued");
    buf->dma_source = NULL;
  }

  buf_list->frames++;
  return buf;

error:
  return NULL;
}

int buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue)
{
  return buf_list->device->hw->buffer_list_pollfd(buf_list, pollfd, can_dequeue);
}
