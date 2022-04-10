#include "device/v4l2/v4l2.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"

int v4l2_buffer_open(buffer_t *buf)
{
  struct v4l2_buffer v4l2_buf = {0};
  struct v4l2_plane v4l2_plane = {0};

  buffer_list_t *buf_list = buf->buf_list;
  device_t *dev = buf_list->device;

	v4l2_buf.type = buf_list->v4l2.type;
  v4l2_buf.index = buf->index;

  if (buf_list->v4l2.do_mplanes) {
    v4l2_buf.length = 1;
    v4l2_buf.m.planes = &v4l2_plane;
    v4l2_plane.data_offset = 0;
  }

  if (buf_list->do_mmap) {
    v4l2_buf.memory = V4L2_MEMORY_MMAP;
  } else {
    v4l2_buf.memory = V4L2_MEMORY_DMABUF;
  }

  E_XIOCTL(buf_list, dev->v4l2.dev_fd, VIDIOC_QUERYBUF, &v4l2_buf, "Cannot query buffer %d", index);

  uint64_t mem_offset = 0;

  if (buf_list->v4l2.do_mplanes) {
    mem_offset = v4l2_plane.m.mem_offset;
    buf->length = v4l2_plane.length;
  } else {
    mem_offset = v4l2_buf.m.offset;
    buf->length = v4l2_buf.length;
  }

  if (buf_list->do_mmap) {
    buf->start = mmap(NULL, buf->length, PROT_READ | PROT_WRITE, MAP_SHARED, dev->v4l2.dev_fd, mem_offset);
    if (buf->start == MAP_FAILED) {
      goto error;
    }

    struct v4l2_exportbuffer v4l2_exp = {0};
    v4l2_exp.type = v4l2_buf.type;
    v4l2_exp.index = buf->index;
    v4l2_exp.plane = 0;
    E_XIOCTL(buf_list, dev->v4l2.dev_fd, VIDIOC_EXPBUF, &v4l2_exp, "Can't export queue buffer=%u to DMA", index);
    buf->dma_fd = v4l2_exp.fd;
  }

  return 0;

error:
  v4l2_buffer_close(buf);
  return -1;
}

void v4l2_buffer_close(buffer_t *buf)
{
  if (buf->start && buf->start != MAP_FAILED) {
    munmap(buf->start, buf->length);
    buf->start = NULL;
  }
  if (buf->dma_fd >= 0) {
    close(buf->dma_fd);
    buf->dma_fd = -1;
  }
}

int v4l2_buffer_enqueue(buffer_t *buf, const char *who)
{
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

  E_XIOCTL(buf, buf->buf_list->device->v4l2.dev_fd, VIDIOC_QBUF, &v4l2_buf, "Can't queue buffer.");

  return 0;

error:
  return -1;
}

int v4l2_buffer_list_dequeue(buffer_list_t *buf_list, buffer_t **bufp)
{
	struct v4l2_buffer v4l2_buf = {0};
	struct v4l2_plane v4l2_plane = {0};

	v4l2_buf.type = buf_list->v4l2.type;
  v4l2_buf.memory = V4L2_MEMORY_MMAP;

	if (buf_list->v4l2.do_mplanes) {
		v4l2_buf.length = 1;
		v4l2_buf.m.planes = &v4l2_plane;
	}

	E_XIOCTL(buf_list, buf_list->device->v4l2.dev_fd, VIDIOC_DQBUF, &v4l2_buf, "Can't grab capture buffer (flags=%08x)", v4l2_buf.flags);

  buffer_t *buf = *bufp = buf_list->bufs[v4l2_buf.index];
	if (buf_list->v4l2.do_mplanes) {
    buf->used = v4l2_plane.bytesused;
  } else {
    buf->used = v4l2_buf.bytesused;
  }

  buf->v4l2.flags = v4l2_buf.flags;
  buf->captured_time_us = get_time_us(CLOCK_FROM_PARAMS, NULL, &v4l2_buf.timestamp, 0);
  return 0;

error:
  return -1;
}

int v4l2_buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue)
{
  int count_enqueued = buffer_list_count_enqueued(buf_list);

  // Can something be dequeued?
  pollfd->fd = buf_list->device->v4l2.dev_fd;
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
