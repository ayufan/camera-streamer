#pragma once

#include "v4l2.h"

typedef struct device_s {
  char *name;
  char *path;
  int fd;
  struct v4l2_capability v4l2_cap;

  struct buffer_list_s *capture_list;
  struct buffer_list_s *output_list;
} device_t;

device_t *device_open(const char *name, const char *path);
void device_close(device_t *device);

int device_open_buffer_list(device_t *dev, bool do_capture, unsigned width, unsigned height, unsigned format, int nbufs, bool allow_dma);
int device_consume_event(device_t *device);

int device_stream(device_t *dev, bool do_on);
