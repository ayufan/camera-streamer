#pragma once

#ifdef USE_MPP

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <linux/videodev2.h>

#include <rockchip/rk_mpi.h>
#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_packet.h>

#ifndef V4L2_PIX_FMT_HEVC
#define V4L2_PIX_FMT_HEVC v4l2_fourcc('H', 'E', 'V', 'C')
#endif

#ifndef V4L2_PIX_FMT_VP8
#define V4L2_PIX_FMT_VP8 v4l2_fourcc('V', 'P', '8', '0')
#endif

#ifndef V4L2_PIX_FMT_VP9
#define V4L2_PIX_FMT_VP9 v4l2_fourcc('V', 'P', '9', '0')
#endif

typedef struct buffer_s buffer_t;
typedef struct buffer_list_s buffer_list_t;
typedef struct device_option_s device_option_t;
typedef struct device_s device_t;
struct pollfd;

typedef int device_option_fn(device_option_t *option, void *opaque);

typedef enum {
  MPP_CODEC_TYPE_DECODER,
  MPP_CODEC_TYPE_ENCODER
} mpp_codec_type_t;

typedef enum {
  MPP_OUTPUT_FORMAT_NV12,
  MPP_OUTPUT_FORMAT_RGB,
  MPP_OUTPUT_FORMAT_JPEG,
  MPP_OUTPUT_FORMAT_H264
} mpp_output_format_t;

typedef struct device_mpp_s {
  MppCtx ctx;
  MppApi *mpi;
  MppBufferGroup buf_grp;
  mpp_codec_type_t codec_type;
  MppCodingType coding_type;
  MppFrameFormat frame_fmt;
  unsigned input_format;
  unsigned output_format;
  int width;
  int height;
  int hor_stride;
  int ver_stride;
  int bitrate;
  int fps;
  bool initialized;
} device_mpp_t;

typedef struct buffer_list_mpp_s {
  MppBufferGroup buf_grp;
  bool do_capture;
  MppFrameFormat frame_fmt;
} buffer_list_mpp_t;

typedef struct buffer_mpp_s {
  MppBuffer mpp_buf;
  MppFrame frame;
  MppPacket packet;
  void *data;
  size_t size;
} buffer_mpp_t;

int mpp_device_open(device_t *dev);
void mpp_device_close(device_t *dev);
int mpp_device_video_force_key(device_t *dev);
void mpp_device_dump_options(device_t *dev, FILE *stream);
int mpp_device_dump_options2(device_t *dev, device_option_fn fn, void *opaque);
int mpp_device_set_fps(device_t *dev, int desired_fps);
int mpp_device_set_option(device_t *dev, const char *key, const char *value);

int mpp_buffer_open(buffer_t *buf);
void mpp_buffer_close(buffer_t *buf);
int mpp_buffer_enqueue(buffer_t *buf, const char *who);
int mpp_buffer_list_dequeue(buffer_list_t *buf_list, buffer_t **bufp);
int mpp_buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue);

int mpp_buffer_list_open(buffer_list_t *buf_list);
void mpp_buffer_list_close(buffer_list_t *buf_list);
int mpp_buffer_list_alloc_buffers(buffer_list_t *buf_list);
void mpp_buffer_list_free_buffers(buffer_list_t *buf_list);
int mpp_buffer_list_set_stream(buffer_list_t *buf_list, bool do_on);

unsigned mpp_to_v4l2_format(MppFrameFormat fmt);
MppFrameFormat v4l2_to_mpp_format(unsigned v4l2_fmt);
MppCodingType v4l2_to_mpp_coding(unsigned v4l2_fmt);

device_t *device_mpp_decoder_open(const char *name, unsigned input_format, unsigned output_format);
device_t *device_mpp_encoder_open(const char *name, unsigned input_format, unsigned output_format);

#endif
