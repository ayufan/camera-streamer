#pragma once

#include "v4l2.h"

typedef struct device_s {
  char *name;
  char *path;
  int fd;
  struct v4l2_capability v4l2_cap;
  bool allow_dma;

  struct buffer_list_s *capture_list;
  struct buffer_list_s *output_list;

  struct device_s *output_device;
} device_t;

device_t *device_open(const char *name, const char *path);
void device_close(device_t *device);

int device_open_buffer_list(device_t *dev, bool do_capture, unsigned width, unsigned height, unsigned format, unsigned bytesperline, int nbufs);
int device_consume_event(device_t *device);

int device_stream(device_t *dev, bool do_on);
int device_video_force_key(device_t *dev);
int device_set_option(device_t *dev, const char *name, uint32_t id, int32_t value);
int device_set_fps(device_t *dev, int desired_fps);

#define DEVICE_SET_OPTION(dev, name, value) \
  device_set_option(dev, #name, V4L2_CID_##name, value)

#define DEVICE_SET_OPTION2(dev, type, name, value) \
  device_set_option(dev, #name, V4L2_CID_##type##_##name, value)

#define DEVICE_SET_OPTION2_FATAL(dev, type, name, value) \
  do { if (DEVICE_SET_OPTION2(dev, type, name, value) < 0) return -1; } while(0)
