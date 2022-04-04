#pragma once

#include "v4l2.h"

typedef struct buffer_list_s {
  char *name;
  struct device_s *device;
  buffer_t **bufs;
  int nbufs;
  int type;

  struct v4l2_format v4l2_format;
  bool do_mplanes;
  bool do_mmap;
  bool do_dma;
  bool do_capture;
} buffer_list_t;

buffer_list_t *buffer_list_open(const char *name, struct device_s *dev, unsigned type);
void buffer_list_close(buffer_list_t *buf_list);

int buffer_list_set_format(buffer_list_t *buffer_list, unsigned width, unsigned height, unsigned format);
int buffer_list_request(buffer_list_t *buf_list, int nbufs);

bool buffer_list_wait_pool(buffer_list_t *buf_list, int timeout);

buffer_t *buffer_list_output_enqueue(buffer_list_t *buf_list, buffer_t *dma_buf);
buffer_t *buffer_list_capture_dequeue(buffer_list_t *buf_list);

int buffer_list_stream(buffer_list_t *buf_list, bool do_on);
