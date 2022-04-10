#include "device/hw/buffer.h"
#include "device/hw/buffer_list.h"
#include "device/hw/device.h"
#include "device/hw/v4l2.h"

buffer_t *buffer_open(const char *name, buffer_list_t *buf_list, int index) {
  buffer_t *buf = calloc(1, sizeof(buffer_t));
  device_t *dev = buf_list->device;

  buf->name = strdup(name);
  buf->index = index;
  buf->buf_list = buf_list;
  buf->dma_fd = -1;
  buf->mmap_reflinks = 1;
  buf->used = 0;

  struct v4l2_buffer v4l2_buf = {0};
  struct v4l2_plane v4l2_plane = {0};

	v4l2_buf.type = buf_list->v4l2.type;
  v4l2_buf.index = index;

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

  E_XIOCTL(buf_list, dev->fd, VIDIOC_QUERYBUF, &v4l2_buf, "Cannot query buffer %d", index);

  uint64_t mem_offset = 0;

  if (buf_list->v4l2.do_mplanes) {
    mem_offset = v4l2_plane.m.mem_offset;
    buf->length = v4l2_plane.length;
  } else {
    mem_offset = v4l2_buf.m.offset;
    buf->length = v4l2_buf.length;
  }

  if (buf_list->do_mmap) {
    buf->start = mmap(NULL, buf->length, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, mem_offset);
    if (buf->start == MAP_FAILED) {
      goto error;
    }
  }

  if (buf_list->do_dma) {
    struct v4l2_exportbuffer v4l2_exp = {0};
    v4l2_exp.type = v4l2_buf.type;
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
