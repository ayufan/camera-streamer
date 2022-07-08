#pragma once

#include <time.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct buffer_s buffer_t;
typedef struct buffer_list_s buffer_list_t;
typedef struct device_s device_t;
struct pollfd;

typedef struct device_v4l2_control_s {
  int fd;
  struct v4l2_query_ext_ctrl control;
} device_v4l2_control_t;

typedef struct device_v4l2_s {
  int dev_fd;
  int subdev_fd;
  device_v4l2_control_t *controls;
  int ncontrols;
} device_v4l2_t;

typedef struct buffer_list_v4l2_s {
  int dev_fd;
  bool do_mplanes;
  int type;
} buffer_list_v4l2_t;

typedef struct buffer_v4l2_s {
  unsigned flags;
} buffer_v4l2_t;

int v4l2_device_open(device_t *dev);
void v4l2_device_close(device_t *dev);
int v4l2_device_set_decoder_start(device_t *dev, bool do_on);
int v4l2_device_video_force_key(device_t *dev);
void v4l2_device_dump_options(device_t *dev, FILE *stream);
int v4l2_device_set_fps(device_t *dev, int desired_fps);
int v4l2_device_set_option(device_t *dev, const char *key, const char *value);

int v4l2_buffer_open(buffer_t *buf);
void v4l2_buffer_close(buffer_t *buf);
int v4l2_buffer_enqueue(buffer_t *buf, const char *who);
int v4l2_buffer_list_dequeue(buffer_list_t *buf_list, buffer_t **bufp);
int v4l2_buffer_list_refresh_states(buffer_list_t *buf_list);
int v4l2_buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue);

int v4l2_buffer_list_open(buffer_list_t *buf_list);
void v4l2_buffer_list_close(buffer_list_t *buf_list);
int v4l2_buffer_list_set_stream(buffer_list_t *buf_list, bool do_on);

int v4l2_device_open_media_device(device_t *dev);
int v4l2_device_open_v4l2_subdev(device_t *dev, int subdev);
int v4l2_device_set_pad_format(device_t *dev, unsigned width, unsigned height, unsigned format);
void v4l2_device_query_controls(device_t *dev, int fd);
