#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct buffer_s buffer_t;
typedef struct device_s device_t;
struct pollfd;

typedef struct buffer_list_s {
  char *name;
  device_t *dev;
  buffer_t **bufs;
  int nbufs;

  bool do_mmap, do_capture;

  union {
    struct buffer_list_v4l2_s *v4l2;
    struct buffer_list_dummy_s *dummy;
    struct buffer_list_libcamera_s *libcamera;
  };

  unsigned fmt_width, fmt_height, fmt_format, fmt_bytesperline, fmt_interval_us;
  bool do_timestamps;

  uint64_t last_enqueued_us, last_dequeued_us;

  bool streaming;
  int frames;
} buffer_list_t;

buffer_list_t *buffer_list_open(const char *name, struct device_s *dev, unsigned width, unsigned height, unsigned format, unsigned bytesperline, int nbufs, bool do_capture, bool do_mmap);
void buffer_list_close(buffer_list_t *buf_list);

int buffer_list_set_stream(buffer_list_t *buf_list, bool do_on);

int buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue);
buffer_t *buffer_list_find_slot(buffer_list_t *buf_list);
buffer_t *buffer_list_dequeue(buffer_list_t *buf_list);
int buffer_list_count_enqueued(buffer_list_t *buf_list);
int buffer_list_enqueue(buffer_list_t *buf_list, buffer_t *dma_buf);
