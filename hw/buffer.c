#include "hw/buffer.h"
#include "hw/buffer_list.h"
#include "hw/device.h"

buffer_t *buffer_open(const char *name, buffer_list_t *buf_list, int index) {
  buffer_t *buf = calloc(1, sizeof(buffer_t));
  device_t *dev = buf_list->device;

  buf->name = strdup(name);
  buf->index = index;
  buf->buf_list = buf_list;
  buf->dma_fd = -1;
  buf->mmap_reflinks = 1;
  buf->used = 0;

  buf->v4l2_buffer.type = buf_list->type;
  buf->v4l2_buffer.index = index;

  if (buf_list->do_mplanes) {
    buf->v4l2_buffer.length = 1;
    buf->v4l2_buffer.m.planes = &buf->v4l2_plane;
    buf->v4l2_plane.data_offset = 0;
  }

  if (buf_list->do_mmap) {
    buf->v4l2_buffer.memory = V4L2_MEMORY_MMAP;
  } else {
    buf->v4l2_buffer.memory = V4L2_MEMORY_DMABUF;
  }

  E_XIOCTL(buf_list, dev->fd, VIDIOC_QUERYBUF, &buf->v4l2_buffer, "Cannot query buffer %d", index);

  if (buf_list->do_mplanes) {
    buf->offset = buf->v4l2_plane.m.mem_offset;
    buf->length = buf->v4l2_plane.length;
  } else {
    buf->offset = buf->v4l2_buffer.m.offset;
    buf->length = buf->v4l2_buffer.length;
  }

  if (buf_list->do_mmap) {
    buf->start = mmap(NULL, buf->length, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, buf->offset);
    if (buf->start == MAP_FAILED) {
      goto error;
    }
  }

  if (buf_list->do_dma) {
    struct v4l2_exportbuffer v4l2_exp = {0};
    v4l2_exp.type = buf_list->type;
    v4l2_exp.index = index;
    v4l2_exp.plane = 0;
    E_XIOCTL(buf_list, dev->fd, VIDIOC_EXPBUF, &v4l2_exp, "Can't export queue buffer=%u to DMA", index);
    buf->dma_fd = v4l2_exp.fd;
  }

  return buf;

error:
  buffer_close(buf);
  return NULL;
}

void buffer_close(buffer_t *buf)
{
  if (buf == NULL) {
    return;
  }

  if (buf->start && buf->start != MAP_FAILED) {
    munmap(buf->start, buf->length);
  }
  if (buf->dma_fd >= 0) {
    close(buf->dma_fd);
  }
  free(buf->name);
  free(buf);
}
