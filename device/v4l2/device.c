#include "device/v4l2/v4l2.h"
#include "device/hw/v4l2.h"
#include "device/hw/device.h"

device_hw_t v4l2_device_hw = {
  .buffer_open = v4l2_buffer_open,
  .buffer_close = v4l2_buffer_close,
  .buffer_enqueue = v4l2_buffer_enqueue,
  .buffer_list_dequeue = v4l2_buffer_list_dequeue,
  .buffer_list_pollfd = v4l2_buffer_list_pollfd,
  .buffer_list_set_format = v4l2_buffer_list_set_format,
  .buffer_list_set_buffers = v4l2_buffer_list_set_buffers,
  .buffer_list_set_stream = v4l2_buffer_list_set_stream
};

device_t *device_v4l2_open(const char *name, const char *path)
{
  return device_open(name, path, &v4l2_device_hw);
}
