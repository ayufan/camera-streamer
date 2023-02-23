#include "dummy.h"

#include "device/device.h"

device_hw_t dummy_device_hw = {
  .device_open = dummy_device_open,
  .device_close = dummy_device_close,
  .device_video_force_key = dummy_device_video_force_key,
  .device_set_fps = dummy_device_set_fps,
  .device_set_option = dummy_device_set_option,

  .buffer_open = dummy_buffer_open,
  .buffer_close = dummy_buffer_close,
  .buffer_enqueue = dummy_buffer_enqueue,

  .buffer_list_dequeue = dummy_buffer_list_dequeue,
  .buffer_list_pollfd = dummy_buffer_list_pollfd,
  .buffer_list_open = dummy_buffer_list_open,
  .buffer_list_close = dummy_buffer_list_close,
  .buffer_list_set_stream = dummy_buffer_list_set_stream
};

device_t *device_dummy_open(const char *name, const char *path)
{
  return device_open(name, path, &dummy_device_hw);
}
