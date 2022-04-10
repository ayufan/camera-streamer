#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <linux/videodev2.h>

typedef struct buffer_s buffer_t;
typedef struct buffer_list_s buffer_list_t;

typedef struct buffer_s {
  char *name;
  struct buffer_list_s *buf_list;
  int index;

  // Mapped memory (with DMA)
  void *start;
  size_t used;
  size_t length;
  int dma_fd;

  struct {
    unsigned flags;
  } v4l2;

  // State
  int mmap_reflinks;
  buffer_t *dma_source;
  bool enqueued;
  uint64_t enqueue_time_us, captured_time_us;
} buffer_t;

buffer_t *buffer_open(const char *name, buffer_list_t *buf_list, int buffer);
void buffer_close(buffer_t *buf);

bool buffer_use(buffer_t *buf);
bool buffer_consumed(buffer_t *buf, const char *who);
