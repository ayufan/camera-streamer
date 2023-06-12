#include "sw.h"

#include "device/device.h"

device_hw_t sw_device_hw = {
  .device_open = sw_device_open,
  .device_close = sw_device_close,

  .buffer_open = sw_buffer_open,
  .buffer_close = sw_buffer_close,
  .buffer_enqueue = sw_buffer_enqueue,

  .buffer_list_dequeue = sw_buffer_list_dequeue,
  .buffer_list_pollfd = sw_buffer_list_pollfd,
  .buffer_list_open = sw_buffer_list_open,
  .buffer_list_close = sw_buffer_list_close,
  .buffer_list_set_stream = sw_buffer_list_set_stream
};

device_t *device_sw_open(const char *name)
{
  return device_open(name, "/dev/null", &sw_device_hw);
}
