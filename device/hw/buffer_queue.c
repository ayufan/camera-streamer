#include "device/hw/buffer.h"
#include "device/hw/buffer_list.h"
#include "device/hw/device.h"
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
    struct v4l2_buffer v4l2_buf = {0};
    struct v4l2_plane v4l2_plane = {0};

    v4l2_buf.type = buf->buf_list->v4l2.type;
    v4l2_buf.index = buf->index;
    v4l2_buf.flags = buf->v4l2.flags;

    if (buf->buf_list->do_mmap) {
      assert(buf->dma_source == NULL);
      v4l2_buf.memory = V4L2_MEMORY_MMAP;
    } else {
      assert(buf->dma_source != NULL);
      v4l2_buf.memory = V4L2_MEMORY_DMABUF;
    }

    // update used bytes
    if (buf->buf_list->v4l2.do_mplanes) {
      v4l2_buf.length = 1;
      v4l2_buf.m.planes = &v4l2_plane;
      v4l2_plane.bytesused = buf->used;
      v4l2_plane.length = buf->length;
      v4l2_plane.data_offset = 0;

      if (buf->dma_source) {
        assert(!buf->buf_list->do_mmap);
        v4l2_plane.m.fd = buf->dma_source->dma_fd;
      }
    } else {
      v4l2_buf.bytesused = buf->used;

      if (buf->dma_source) {
        assert(!buf->buf_list->do_mmap);
        v4l2_buf.m.fd = buf->dma_source->dma_fd;
      }
    }

    E_LOG_DEBUG(buf, "Queuing buffer... used=%zu length=%zu (linked=%s) by %s",
      buf->used,
      buf->length,
      buf->dma_source ? buf->dma_source->name : NULL,
      who);

    // Assign or clone timestamp
    if (buf->buf_list->do_timestamps) {
      get_monotonic_time_us(NULL, &v4l2_buf.timestamp);
    } else {
      v4l2_buf.timestamp.tv_sec = buf->captured_time_us / (1000LL * 1000LL);
      v4l2_buf.timestamp.tv_usec = buf->captured_time_us % (1000LL * 1000LL);
    }

    E_XIOCTL(buf, buf->buf_list->device->fd, VIDIOC_QBUF, &v4l2_buf, "Can't queue buffer.");
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
	struct v4l2_buffer v4l2_buf = {0};
	struct v4l2_plane v4l2_plane = {0};

	v4l2_buf.type = buf_list->v4l2.type;
  v4l2_buf.memory = V4L2_MEMORY_MMAP;

	if (buf_list->v4l2.do_mplanes) {
		v4l2_buf.length = 1;
		v4l2_buf.m.planes = &v4l2_plane;
	}

	E_XIOCTL(buf_list, buf_list->device->fd, VIDIOC_DQBUF, &v4l2_buf, "Can't grab capture buffer (flags=%08x)", v4l2_buf.flags);

  buffer_t *buf = buf_list->bufs[v4l2_buf.index];
	if (buf_list->v4l2.do_mplanes) {
    buf->used = v4l2_plane.bytesused;
  } else {
    buf->used = v4l2_buf.bytesused;
  }
  // TODO: Copy flags
  buf->v4l2.flags = v4l2_buf.flags;
  buf->captured_time_us = get_time_us(CLOCK_FROM_PARAMS, NULL, &v4l2_buf.timestamp, 0);
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
  int count_enqueued = buffer_list_count_enqueued(buf_list);

  // Can something be dequeued?
  pollfd->fd = buf_list->device->fd;
  pollfd->events = POLLHUP;
  if (count_enqueued > 0 && can_dequeue) {
    if (buf_list->do_capture)
      pollfd->events |= POLLIN;
    else
      pollfd->events |= POLLOUT;
  }
  pollfd->revents = 0;
  return 0;
}
