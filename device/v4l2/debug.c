#include "v4l2.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "opts/log.h"

int v4l2_buffer_list_refresh_states(buffer_list_t *buf_list)
{
  if (!buf_list) {
    return -1;
  }

  struct v4l2_buffer v4l2_buf = {0};
  struct v4l2_plane v4l2_plane = {0};

	v4l2_buf.type = buf_list->v4l2->type;

  if (buf_list->v4l2->do_mplanes) {
    v4l2_buf.length = 1;
    v4l2_buf.m.planes = &v4l2_plane;
  }

  if (buf_list->do_mmap) {
    v4l2_buf.memory = V4L2_MEMORY_MMAP;
  } else {
    v4l2_buf.memory = V4L2_MEMORY_DMABUF;
  }

  for (int i = 0; i < buf_list->nbufs; i++) {
    v4l2_buf.index = i;

  	E_XIOCTL(buf_list, buf_list->dev->v4l2->dev_fd, VIDIOC_QUERYBUF, &v4l2_buf, "Can't query buffer (flags=%08x)", i);
    E_LOG_INFO(buf_list, "Buffer: %d, Flags: %08x. Offset: %d", i, v4l2_buf.flags,
      buf_list->v4l2->do_mplanes ? v4l2_plane.m.mem_offset : v4l2_buf.m.offset);
  }

error:
  return -1;
}
