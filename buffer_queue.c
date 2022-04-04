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

bool buffer_capture_enqueue(buffer_t *buf)
{
  if (buf->enqueued || !buf->buf_list->do_mmap || !buf->buf_list->do_capture) {
    return false;
  }

  E_LOG_DEBUG(buf, "Queuing buffer...");
  E_XIOCTL(buf, buf->buf_list->device->fd, VIDIOC_QBUF, &buf->v4l2_buffer, "Can't queue buffer.");

  buf->enqueued = true;

error:
  return true;
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

buffer_t *buffer_list_capture_dequeue(buffer_list_t *buf_list)
{
  if (!buf_list->do_mmap || !buf_list->do_capture) {
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
	E_LOG_DEBUG(buf_list, "Grabbed INPUT buffer=%u, bytes=%d",
    v4l2_buf.index, v4l2_buf.length);

  buffer_t *buf = buf_list->bufs[v4l2_buf.index];
  buf->v4l2_plane = v4l2_plane;
  buf->v4l2_buffer = v4l2_buf;
	if (buf_list->do_mplanes) {
		buf->v4l2_buffer.m.planes = &buf->v4l2_plane;
  }

  buf->enqueued = false;

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