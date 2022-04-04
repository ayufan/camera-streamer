#include "buffer.h"
#include "buffer_list.h"
#include "device.h"

bool buffer_output_dequeue(buffer_t *buf)
{
  // if (buf->enqueued || buf->buf_list->do_capture) {
  //   return false;
  // }

  return false;
}

bool buffer_consumed(buffer_t *buf)
{
  if (buf->mmap_reflinks > 0) {
    buf->mmap_reflinks--;

    // update used bytes
    if (buf->buf_list->do_mplanes) {
      buf->v4l2_plane.bytesused = buf->used;
    } else {
      buf->v4l2_buffer.bytesused = buf->used;
    }

    if (buf->mmap_reflinks == 0) {
      E_LOG_DEBUG(buf, "Queuing buffer...");
      E_XIOCTL(buf, buf->buf_list->device->fd, VIDIOC_QBUF, &buf->v4l2_buffer, "Can't queue buffer.");
    }
  }

  if (buf->mmap_source) {
    buffer_consumed(buf->mmap_source);
    buf->mmap_source = NULL;
  }

  return true;

error:
  return false;
}

buffer_t *buffer_list_output_enqueue(buffer_list_t *buf_list, buffer_t *dma_buf)
{
  // if (dma_buf->enqueued || dma_buf->dma_fd < 0) {
  //   return NULL;
  // }

	// struct v4l2_buffer v4l2_buf = {0};
	// struct v4l2_plane v4l2_plane = {0};

	// v4l2_buf.type = buf_list->type;
  // v4l2_buf.memory = V4L2_MEMORY_MMAP;

	// if (buf_list->do_mplanes) {
	// 	v4l2_buf.length = 1;
	// 	v4l2_buf.m.planes = &v4l2_plane;
	// }

	// E_XIOCTL(buf_list, buf_list->device->fd, VIDIOC_DQBUF, &v4l2_buf, "Can't grab capture buffer");
	// E_LOG_DEBUG(buf_list, "Grabbed INPUT buffer=%u", v4l2_buf.index);

  return NULL;
}

buffer_t *buffer_list_mmap_enqueue(buffer_list_t *buf_list, buffer_t *dma_buf)
{
  buffer_t *buf = NULL;

  if (!buf_list->do_mmap && !dma_buf->buf_list->do_mmap) {
    E_LOG_PERROR(buf_list, "Cannot enqueue non-mmap to non-mmap: %s.", dma_buf->name);
  }

  if (!buf_list->do_mmap) {
    E_LOG_PERROR(buf_list, "Not yet supported: %s.", dma_buf->name);
  }

  buf = buffer_list_mmap_dequeue(buf_list);
  if (!buf) {
    return NULL;
  }

  if (dma_buf->used > buf->length) {
    E_LOG_PERROR(buf_list, "The dma_buf (%s) is too long: %zu vs space=%zu",
      dma_buf->name, dma_buf->used, buf->length);
  }

  E_LOG_DEBUG(buf, "mmap copy: dest=%p, src=%p (%s), size=%zu, space=%zu",
    buf->start, dma_buf->start, dma_buf->name, dma_buf->used, buf->length);
  memcpy(buf->start, dma_buf->start, dma_buf->used);
  buffer_consumed(buf);

  return NULL;

error:
  return NULL;
}

buffer_t *buffer_list_mmap_dequeue(buffer_list_t *buf_list)
{
  if (!buf_list->do_mmap) {
    return NULL;
  }

	struct v4l2_buffer v4l2_buf = {0};
	struct v4l2_plane v4l2_plane = {0};

	v4l2_buf.type = buf_list->type;
  v4l2_buf.memory = V4L2_MEMORY_MMAP;

	if (buf_list->do_mplanes) {
		v4l2_buf.length = 1;
		v4l2_buf.m.planes = &v4l2_plane;
	}

	E_XIOCTL(buf_list, buf_list->device->fd, VIDIOC_DQBUF, &v4l2_buf, "Can't grab capture buffer");

  buffer_t *buf = buf_list->bufs[v4l2_buf.index];
  buf->v4l2_plane = v4l2_plane;
  buf->v4l2_buffer = v4l2_buf;
	if (buf_list->do_mplanes) {
		buf->v4l2_buffer.m.planes = &buf->v4l2_plane;
    buf->used = buf->v4l2_plane.bytesused;
  } else {
    buf->used = buf->v4l2_buffer.bytesused;
  }

	E_LOG_DEBUG(buf_list, "Grabbed mmap buffer=%u, bytes=%d, used=%d",
    buf->index, buf->length, buf->used);

  buf->mmap_reflinks = 1;

  return buf;

error:
  return NULL;
}

bool buffer_list_wait_pool(buffer_list_t *buf_list, int timeout) {
  struct pollfd fds = {buf_list->device->fd, POLLIN, 0};

  if (poll(&fds, 1, timeout) < 0 && errno != EINTR) {
    E_LOG_ERROR(buf_list, "Can't poll encoder");
  }

  E_LOG_DEBUG(buf_list, "Polling encoder %d, %d...", errno, fds.revents);
	if (fds.revents & POLLIN) {
    return true;
  }
  if (fds.revents & POLLPRI) {
    E_LOG_DEBUG(buf_list, "fd POLLPRI");
  }
  if (fds.revents & POLLOUT) {
    E_LOG_DEBUG(buf_list, "fd POLLOUT");
  }
  if (fds.revents & POLLERR) {
    E_LOG_DEBUG(buf_list, "fd POLLERR");
    device_consume_event(buf_list->device);
  }
  if (fds.revents & POLLHUP) {
    E_LOG_DEBUG(buf_list, "fd POLLHUP");
  }
  if (fds.revents & POLLNVAL) {
    E_LOG_DEBUG(buf_list, "fd POLLNVAL");
  }

  return false;

error:
  return false;
}