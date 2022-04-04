#pragma once

#include "v4l2.h"

typedef struct buffer_s {
  char *name;
  struct buffer_list_s *buf_list;
  int index;
  void *start;
  size_t offset;
  size_t length;
  struct v4l2_buffer v4l2_buffer;
  struct v4l2_plane v4l2_plane;
  int dma_fd;
  bool enqueued;
} buffer_t;

buffer_t *buffer_open(const char *name, struct buffer_list_s *buf_list, int buffer);
void buffer_close(buffer_t *buf);
bool buffer_output_dequeue(buffer_t *buf);
bool buffer_capture_enqueue(buffer_t *buf);
