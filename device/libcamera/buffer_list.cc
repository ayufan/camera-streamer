#include "libcamera.hh"

int libcamera_buffer_list_open(buffer_list_t *buf_list, unsigned width, unsigned height, unsigned format, unsigned bytesperline, int nbufs)
{
  if (!buf_list->do_capture) {
    E_LOG_INFO(buf_list, "Only capture mode is supported.");
    return -1;
  }

  buf_list->libcamera = new buffer_list_libcamera_t{};
  return 0;
}

void libcamera_buffer_list_close(buffer_list_t *buf_list)
{
  if (buf_list->libcamera) {
    delete buf_list->libcamera;
    buf_list->libcamera = NULL;
  }
}

int libcamera_buffer_list_set_stream(buffer_list_t *buf_list, bool do_on)
{
  return -1;
}
