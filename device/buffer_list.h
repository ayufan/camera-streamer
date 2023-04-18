#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct buffer_s buffer_t;
typedef struct device_s device_t;
struct pollfd;

typedef enum {
  BUFFER_TYPE_DEFAULT = 0,

  BUFFER_TYPE_RAW = 1,
  BUFFER_TYPE_IMAGE = 2,
  BUFFER_TYPE_VIDEO = 3
} buffer_type_t;

typedef struct buffer_format_s {
  unsigned width, height, format, bytesperline, sizeimage;
  unsigned nbufs;
  unsigned interval_us;
  buffer_type_t type;
} buffer_format_t;

typedef struct buffer_stats_s {
  int frames, dropped;
} buffer_stats_t;

#define MAX_BUFFER_QUEUE 4

typedef struct buffer_list_s {
  char *name;
  char *path;
  device_t *dev;
  buffer_t **bufs;
  int nbufs;
  int index;

  buffer_format_t fmt;
  bool do_mmap, do_capture, do_timestamps;

  union {
    struct buffer_list_v4l2_s *v4l2;
    struct buffer_list_dummy_s *dummy;
    struct buffer_list_libcamera_s *libcamera;
  };

  buffer_t *queued_bufs[MAX_BUFFER_QUEUE];
  int n_queued_bufs;

  uint64_t last_enqueued_us, last_dequeued_us;
  int last_capture_time_us, last_in_queue_time_us;
  bool streaming;
  buffer_stats_t stats, stats_last;
} buffer_list_t;

buffer_list_t *buffer_list_open(const char *name, int index, struct device_s *dev, const char *path, buffer_format_t fmt, bool do_capture, bool do_mmap);
void buffer_list_close(buffer_list_t *buf_list);
int buffer_list_alloc_buffers(buffer_list_t *buf_list);
void buffer_list_free_buffers(buffer_list_t *buf_list);

int buffer_list_set_stream(buffer_list_t *buf_list, bool do_on);

int buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue);
buffer_t *buffer_list_find_slot(buffer_list_t *buf_list);
buffer_t *buffer_list_dequeue(buffer_list_t *buf_list);
int buffer_list_count_enqueued(buffer_list_t *buf_list);
int buffer_list_enqueue(buffer_list_t *buf_list, buffer_t *dma_buf);
void buffer_list_clear_queue(buffer_list_t *buf_list);
bool buffer_list_push_to_queue(buffer_list_t *buf_list, buffer_t *dma_buf, int max_bufs);
buffer_t *buffer_list_pop_from_queue(buffer_list_t *buf_list);
