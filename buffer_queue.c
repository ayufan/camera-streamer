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

buffer_t *_buffer_list_enqueue_mmap(buffer_list_t *buf_list, buffer_t *dma_buf)
{
  buffer_t *buf = NULL;

  buf = buffer_list_dequeue(buf_list, true);
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

buffer_t *_buffer_list_enqueue_dmabuf(buffer_list_t *buf_list, buffer_t *dma_buf)
{
  buffer_t *buf = NULL;

  for (int i = 0; i < buf_list->nbufs; i++) {
    if (!buf_list->bufs[i]->mmap_source) {
      buf = buf_list->bufs[i];
      break;
    }
  }
  if (!buf) {
    return NULL;
  }

  struct v4l2_buffer v4l2_buf = {0};
  struct v4l2_plane v4l2_plane = {0};

  v4l2_buf.type = buf_list->type;
  v4l2_buf.memory = V4L2_MEMORY_DMABUF;
  v4l2_buf.index = buf->index;

  if (buf_list->do_mplanes) {
    v4l2_buf.length = 1;
    v4l2_buf.m.planes = &v4l2_plane;
    v4l2_plane.m.fd = dma_buf->dma_fd;
		v4l2_plane.bytesused = dma_buf->used;
		v4l2_plane.length = dma_buf->used;
  } else {
    v4l2_buf.m.fd = dma_buf->dma_fd;
		v4l2_buf.bytesused = dma_buf->used;
		v4l2_buf.length = dma_buf->used;
  }

	// uint64_t now = get_now_monotonic_u64();
	// struct timeval ts = {
	// 	.tv_sec = now / 1000000,
	// 	.tv_usec = now % 1000000,
	// };

	// v4l2_buf.timestamp.tv_sec = ts.tv_sec;
	// v4l2_buf.timestamp.tv_usec = ts.tv_usec;

  E_LOG_DEBUG(buf, "dmabuf copy: dest=%p, src=%p (%s, dma_fd=%d), size=%zu",
    buf->start, dma_buf->start, dma_buf->name, dma_buf->dma_fd, dma_buf->used);

  E_XIOCTL(buf_list, buf_list->device->fd, VIDIOC_QBUF, &v4l2_buf, "Can't push DMA buffer");

  buf->mmap_source = dma_buf;
  buf_list->frames++;
  return buf;
error:
  return NULL;
}

buffer_t *buffer_list_enqueue(buffer_list_t *buf_list, buffer_t *dma_buf)
{
  buffer_t *buf = NULL;

  if (!buf_list->do_mmap && !dma_buf->buf_list->do_mmap) {
    E_LOG_PERROR(buf_list, "Cannot enqueue non-mmap to non-mmap: %s.", dma_buf->name);
  }

  if (buf_list->do_mmap) {
    return _buffer_list_enqueue_mmap(buf_list, dma_buf);
  } else {
    return _buffer_list_enqueue_dmabuf(buf_list, dma_buf);
  }

error:
  return NULL;
}

buffer_t *buffer_list_dequeue(buffer_list_t *buf_list, int mmap)
{
  if (mmap != buf_list->do_mmap) {
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

	E_LOG_DEBUG(buf_list, "Grabbed mmap buffer=%u, bytes=%d, used=%d, frame=%d",
    buf->index, buf->length, buf->used, buf_list->frames);

  if (buf_list->do_mmap) {
    buf->mmap_reflinks = 1;
    buf_list->frames++;
  } else {
    buf->mmap_reflinks = 0;
  }

  return buf;

error:
  return NULL;
}
