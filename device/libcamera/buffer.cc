#include "libcamera.hh"

extern "C" {
#include "device/buffer.h"
#include <stdlib.h>
};

int libcamera_buffer_open(buffer_t *buf)
{
  buf->libcamera = new buffer_libcamera_t{};
  return 0;
}

void libcamera_buffer_close(buffer_t *buf)
{
  if (buf->libcamera) {
    delete buf->libcamera;
    buf->libcamera = NULL;
  }
}

int libcamera_buffer_enqueue(buffer_t *buf, const char *who)
{
  return -1;
}

int libcamera_buffer_list_dequeue(buffer_list_t *buf_list, buffer_t **bufp)
{
  return -1;
}

int libcamera_buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue)
{
  return -1;
}
