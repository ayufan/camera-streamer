#include "libcamera.hh"

#ifdef USE_LIBCAMERA
device_hw_t libcamera_device_hw = {
  .device_open = libcamera_device_open,
  .device_close = libcamera_device_close,
  .device_set_fps = libcamera_device_set_fps,
  .device_set_option = libcamera_device_set_option,

  .buffer_open = libcamera_buffer_open,
  .buffer_close = libcamera_buffer_close,
  .buffer_enqueue = libcamera_buffer_enqueue,

  .buffer_list_dequeue = libcamera_buffer_list_dequeue,
  .buffer_list_pollfd = libcamera_buffer_list_pollfd,
  .buffer_list_open = libcamera_buffer_list_open,
  .buffer_list_close = libcamera_buffer_list_close,
  .buffer_list_set_stream = libcamera_buffer_list_set_stream
};

extern "C" device_t *device_libcamera_open(const char *name, const char *path)
{
  return device_open(name, path, &libcamera_device_hw);
}
#else // USE_LIBCAMERA
extern "C" device_t *device_libcamera_open(const char *name, const char *path)
{
  E_LOG_INFO(NULL, "libcamera is not supported");
  return NULL;
}
#endif // USE_LIBCAMERA
