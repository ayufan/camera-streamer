#include "dummy.h"
#include "device/device.h"

#include <stdlib.h>

int dummy_device_open(device_t *dev)
{
  dev->opts.allow_dma = false;
  dev->dummy = calloc(1, sizeof(device_dummy_t));
  return 0;
}

void dummy_device_close(device_t *dev)
{
  free(dev->dummy);
}

int dummy_device_video_force_key(device_t *dev)
{
  return -1;
}

int dummy_device_set_fps(device_t *dev, int desired_fps)
{
  return -1;
}

int dummy_device_set_option(device_t *dev, const char *key, const char *value)
{
  return -1;
}
