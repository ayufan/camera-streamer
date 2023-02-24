#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct buffer_s buffer_t;
typedef struct buffer_list_s buffer_list_t;
typedef struct buffer_format_s buffer_format_t;
typedef struct device_s device_t;
struct pollfd;

typedef struct device_hw_s {
  int (*device_open)(device_t *dev);
  void (*device_close)(device_t *dev);
  int (*device_video_force_key)(device_t *dev);
  void (*device_dump_options)(device_t *dev, FILE *stream);
  int (*device_set_fps)(device_t *dev, int desired_fps);
  int (*device_set_rotation)(device_t *dev, bool vflip, bool hflip);
  int (*device_set_option)(device_t *dev, const char *key, const char *value);

  int (*buffer_open)(buffer_t *buf);
  void (*buffer_close)(buffer_t *buf);
  int (*buffer_enqueue)(buffer_t *buf, const char *who);
  int (*buffer_list_dequeue)(buffer_list_t *buf_list, buffer_t **bufp);
  int (*buffer_list_pollfd)(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue);

  int (*buffer_list_open)(buffer_list_t *buf_list);
  void (*buffer_list_close)(buffer_list_t *buf_list);
  int (*buffer_list_set_stream)(buffer_list_t *buf_list, bool do_on);
} device_hw_t;

typedef struct device_s {
  char *name;
  char *path;
  char bus_info[64];

  device_hw_t *hw;
  int n_capture_list;
  buffer_list_t **capture_lists;
  buffer_list_t *output_list;

  struct {
    bool allow_dma;
  } opts;

  union {
    struct device_v4l2_s *v4l2;
    struct device_dummy_s *dummy;
    struct device_libcamera_s *libcamera;
  };

  bool paused;
} device_t;

device_t *device_open(const char *name, const char *path, device_hw_t *hw);
void device_close(device_t *dev);

buffer_list_t *device_open_buffer_list(device_t *dev, bool do_capture, buffer_format_t fmt, bool do_mmap);
buffer_list_t *device_open_buffer_list2(device_t *dev, const char *path, bool do_capture, buffer_format_t fmt, bool do_mmap);
buffer_list_t *device_open_buffer_list_output(device_t *dev, buffer_list_t *capture_list);
buffer_list_t *device_open_buffer_list_capture(device_t *dev, const char *path, buffer_list_t *output_list, buffer_format_t fmt, bool do_mmap);
buffer_list_t *device_open_buffer_list_capture2(device_t *dev, const char *path, buffer_list_t *output_list, unsigned choosen_format, bool do_mmap);

int device_set_stream(device_t *dev, bool do_on);
int device_video_force_key(device_t *dev);

void device_dump_options(device_t *dev, FILE *stream);
int device_set_fps(device_t *dev, int desired_fps);
int device_set_rotation(device_t *dev, bool vflip, bool hflip);
int device_set_option_string(device_t *dev, const char *option, const char *value);
void device_set_option_list(device_t *dev, const char *option_list);

int device_output_enqueued(device_t *dev);
int device_capture_enqueued(device_t *dev, int *max);

device_t *device_v4l2_open(const char *name, const char *path);
device_t *device_libcamera_open(const char *name, const char *path);
device_t *device_dummy_open(const char *name, const char *path);
