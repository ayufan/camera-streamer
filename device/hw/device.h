#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <linux/videodev2.h>

typedef struct buffer_s buffer_t;
typedef struct buffer_list_s buffer_list_t;
typedef struct device_s device_t;
struct pollfd;

typedef struct device_hw_s {
  int (*buffer_open)(buffer_t *buf);
  void (*buffer_close)(buffer_t *buf);
  int (*buffer_enqueue)(buffer_t *buf, const char *who);
  int (*buffer_list_dequeue)(buffer_list_t *buf_list, buffer_t **bufp);
  int (*buffer_list_pollfd)(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue);

  int (*buffer_list_set_format)(buffer_list_t *buf_list, unsigned width, unsigned height, unsigned format, unsigned bytesperline);
  int (*buffer_list_set_buffers)(buffer_list_t *buf_list, int nbufs);
  int (*buffer_list_set_stream)(buffer_list_t *buf_list, bool do_on);
} device_hw_t;

typedef struct device_s {
  char *name;
  char *path;
  char bus_info[64];
  int fd;
  int subdev_fd;
  bool allow_dma;

  device_hw_t *hw;
  buffer_list_t *capture_list;
  buffer_list_t *output_list;

  device_t *output_device;
  bool paused;
  bool decoder_started;
} device_t;

device_t *device_open(const char *name, const char *path, device_hw_t *hw);
device_t *device_v4l2_open(const char *name, const char *path);
int device_open_media_device(device_t *dev);
int device_open_v4l2_subdev(device_t *dev, int subdev);
void device_close(device_t *device);

int device_open_buffer_list(device_t *dev, bool do_capture, unsigned width, unsigned height, unsigned format, unsigned bytesperline, int nbufs, bool do_mmap);
int device_open_buffer_list_output(device_t *dev, buffer_list_t *capture_list);
int device_open_buffer_list_capture(device_t *dev, buffer_list_t *output_list, float div, unsigned format, bool do_mmap);
int device_consume_event(device_t *device);

int device_set_stream(device_t *dev, bool do_on);
int device_set_decoder_start(device_t *dev, bool do_on);
int device_video_force_key(device_t *dev);

int device_set_fps(device_t *dev, int desired_fps);
int device_set_pad_format(device_t *device, unsigned width, unsigned height, unsigned format);
int device_set_option(device_t *dev, const char *name, uint32_t id, int32_t value);
int device_set_option_string(device_t *dev, const char *option);
void device_set_option_list(device_t *dev, const char *option_list);

#define DEVICE_SET_OPTION(dev, name, value) \
  device_set_option(dev, #name, V4L2_CID_##name, value)

#define DEVICE_SET_OPTION2(dev, type, name, value) \
  device_set_option(dev, #name, V4L2_CID_##type##_##name, value)

#define DEVICE_SET_OPTION2_FATAL(dev, type, name, value) \
  do { if (DEVICE_SET_OPTION2(dev, type, name, value) < 0) return -1; } while(0)
