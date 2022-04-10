#include "device/hw/v4l2.h"
#include "device/hw/device.h"

extern device_hw_t device_hw_v4l2;

device_t *device_v4l2_open(const char *name, const char *path)
{
  return device_open(name, path, &device_hw_v4l2);
}
