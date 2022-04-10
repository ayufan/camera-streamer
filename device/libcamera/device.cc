#include "libcamera.hh"

extern "C" {
#include "device/device.h"
#include <stdlib.h>
};

int libcamera_device_open(device_t *dev)
{
  dev->libcamera = new device_libcamera_t{};
  return 0;
}

void libcamera_device_close(device_t *dev)
{
  if (dev->libcamera) {
    delete dev->libcamera;
    dev->libcamera = NULL;
  }
}

int libcamera_device_set_decoder_start(device_t *dev, bool do_on)
{
  return -1;
}

int libcamera_device_video_force_key(device_t *dev)
{
  return -1;
}

int libcamera_device_set_fps(device_t *dev, int desired_fps)
{
  return -1;
}

int libcamera_device_set_option(device_t *dev, const char *key, const char *value)
{
  return -1;
}
