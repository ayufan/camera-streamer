#include "v4l2.h"
#include "device/device.h"

device_hw_t v4l2_device_hw = {
  .device_open = v4l2_device_open,
  .device_close = v4l2_device_close,
  .device_set_decoder_start = v4l2_device_set_decoder_start,
  .device_video_force_key = v4l2_device_video_force_key,
  .device_set_fps = v4l2_device_set_fps,
  .device_set_option = v4l2_device_set_option,

  .buffer_open = v4l2_buffer_open,
  .buffer_close = v4l2_buffer_close,
  .buffer_enqueue = v4l2_buffer_enqueue,

  .buffer_list_dequeue = v4l2_buffer_list_dequeue,
  .buffer_list_pollfd = v4l2_buffer_list_pollfd,
  .buffer_list_open = v4l2_buffer_list_open,
  .buffer_list_close = v4l2_buffer_list_close,
  .buffer_list_set_stream = v4l2_buffer_list_set_stream
};

device_t *device_v4l2_open(const char *name, const char *path)
{
  return device_open(name, path, &v4l2_device_hw);
}
