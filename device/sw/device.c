#include "sw.h"
#include "device/device.h"

#include <stdlib.h>

int sw_device_open(device_t *dev)
{
  dev->opts.allow_dma = false;
  dev->sw = calloc(1, sizeof(device_sw_t));
  return 0;
}

void sw_device_close(device_t *dev)
{
  free(dev->sw);
}
