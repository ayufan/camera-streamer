#pragma once

#include "v4l2.h"

typedef struct buffer_s {
  char *name;
  struct buffer_list_s *buf_list;
  int index;
  void *start;
  size_t offset;
  size_t used;
  size_t length;
  struct v4l2_buffer v4l2_buffer;
  struct v4l2_plane v4l2_plane;
  int dma_fd;

  int mmap_reflinks;
  struct buffer_s *mmap_source;
  bool enqueued;
  uint64_t enqueue_time_us;
} buffer_t;

buffer_t *buffer_open(const char *name, struct buffer_list_s *buf_list, int buffer);
void buffer_close(buffer_t *buf);

bool buffer_use(buffer_t *buf);
bool buffer_consumed(buffer_t *buf, const char *who);
