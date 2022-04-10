#include "libcamera.hh"

extern "C" {
#include "device/device.h"
};

device_hw_t libcamera_device_hw = {
  .device_open = libcamera_device_open,
  .device_close = libcamera_device_close,
  .device_set_decoder_start = libcamera_device_set_decoder_start,
  .device_video_force_key = libcamera_device_video_force_key,
  .device_set_fps = libcamera_device_set_fps,
  .device_set_option = libcamera_device_set_option,

  .buffer_open = libcamera_buffer_open,
  .buffer_close = libcamera_buffer_close,
  .buffer_enqueue = libcamera_buffer_enqueue,

  .buffer_list_dequeue = libcamera_buffer_list_dequeue,
  .buffer_list_pollfd = libcamera_buffer_list_pollfd,
  .buffer_list_open = libcamera_buffer_list_open,
  .buffer_list_close = libcamera_buffer_list_close,
  .buffer_list_set_buffers = libcamera_buffer_list_set_buffers,
  .buffer_list_set_stream = libcamera_buffer_list_set_stream
};

extern "C" device_t *device_libcamera_open(const char *name, const char *path)
{
  return device_open(name, path, &libcamera_device_hw);
}
