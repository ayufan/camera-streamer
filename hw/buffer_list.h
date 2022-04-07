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

  unsigned fmt_width, fmt_height, fmt_format, fmt_bytesperline, fmt_interval_us;
  bool do_timestamps;

  uint64_t last_dequeued_us;

  bool streaming;
  int frames;
} buffer_list_t;

buffer_list_t *buffer_list_open(const char *name, struct device_s *dev, unsigned type, bool do_mmap);
void buffer_list_close(buffer_list_t *buf_list);

int buffer_list_set_format(buffer_list_t *buffer_list, unsigned width, unsigned height, unsigned format, unsigned bytesperline);
int buffer_list_request(buffer_list_t *buf_list, int nbufs);

int buffer_list_stream(buffer_list_t *buf_list, bool do_on);

buffer_t *buffer_list_find_slot(buffer_list_t *buf_list);
buffer_t *buffer_list_dequeue(buffer_list_t *buf_list);
int buffer_list_count_enqueued(buffer_list_t *buf_list);

int buffer_list_enqueue(buffer_list_t *buf_list, buffer_t *dma_buf);
int buffer_list_refresh_states(buffer_list_t *buf_list);
