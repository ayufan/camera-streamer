#ifdef USE_MPP

#include "mpp.h"
#include "device/device.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>

unsigned mpp_to_v4l2_format(MppFrameFormat fmt)
{
  switch (fmt) {
  case MPP_FMT_YUV420SP:
    return V4L2_PIX_FMT_NV12;
  case MPP_FMT_YUV420SP_VU:
    return V4L2_PIX_FMT_NV21;
  case MPP_FMT_YUV422_YUYV:
    return V4L2_PIX_FMT_YUYV;
  case MPP_FMT_YUV422_UYVY:
    return V4L2_PIX_FMT_UYVY;
  case MPP_FMT_YUV420P:
    return V4L2_PIX_FMT_YUV420;
  case MPP_FMT_RGB888:
    return V4L2_PIX_FMT_RGB24;
  case MPP_FMT_BGR888:
    return V4L2_PIX_FMT_BGR24;
  case MPP_FMT_ARGB8888:
    return V4L2_PIX_FMT_ARGB32;
  case MPP_FMT_ABGR8888:
    return V4L2_PIX_FMT_ABGR32;
  default:
    return 0;
  }
}

MppFrameFormat v4l2_to_mpp_format(unsigned v4l2_fmt)
{
  switch (v4l2_fmt) {
  case V4L2_PIX_FMT_NV12:
    return MPP_FMT_YUV420SP;
  case V4L2_PIX_FMT_NV21:
    return MPP_FMT_YUV420SP_VU;
  case V4L2_PIX_FMT_YUYV:
    return MPP_FMT_YUV422_YUYV;
  case V4L2_PIX_FMT_UYVY:
    return MPP_FMT_YUV422_UYVY;
  case V4L2_PIX_FMT_YUV420:
    return MPP_FMT_YUV420P;
  case V4L2_PIX_FMT_RGB24:
    return MPP_FMT_RGB888;
  case V4L2_PIX_FMT_BGR24:
    return MPP_FMT_BGR888;
  case V4L2_PIX_FMT_ARGB32:
    return MPP_FMT_ARGB8888;
  case V4L2_PIX_FMT_ABGR32:
    return MPP_FMT_ABGR8888;
  default:
    return MPP_FMT_YUV420SP;
  }
}

MppCodingType v4l2_to_mpp_coding(unsigned v4l2_fmt)
{
  switch (v4l2_fmt) {
  case V4L2_PIX_FMT_MJPEG:
  case V4L2_PIX_FMT_JPEG:
    return MPP_VIDEO_CodingMJPEG;
  case V4L2_PIX_FMT_H264:
    return MPP_VIDEO_CodingAVC;
  case V4L2_PIX_FMT_HEVC:
    return MPP_VIDEO_CodingHEVC;
  case V4L2_PIX_FMT_VP8:
    return MPP_VIDEO_CodingVP8;
  case V4L2_PIX_FMT_VP9:
    return MPP_VIDEO_CodingVP9;
  default:
    return MPP_VIDEO_CodingUnused;
  }
}

int mpp_device_open(device_t *dev)
{
  dev->mpp = calloc(1, sizeof(device_mpp_t));
  if (!dev->mpp) {
    LOG_ERROR(dev, "Failed to allocate MPP device structure");
    goto error;
  }

  dev->mpp->initialized = false;
  dev->mpp->fps = 30;
  dev->mpp->bitrate = 4000000;

  LOG_INFO(dev, "MPP device opened: %s", dev->path);
  return 0;

error:
  return -1;
}

void mpp_device_close(device_t *dev)
{
  if (!dev->mpp)
    return;

  if (dev->mpp->ctx) {
    dev->mpp->mpi->reset(dev->mpp->ctx);
    mpp_destroy(dev->mpp->ctx);
    dev->mpp->ctx = NULL;
    dev->mpp->mpi = NULL;
  }

  if (dev->mpp->buf_grp) {
    mpp_buffer_group_put(dev->mpp->buf_grp);
    dev->mpp->buf_grp = NULL;
  }

  free(dev->mpp);
  dev->mpp = NULL;

  LOG_INFO(dev, "MPP device closed");
}

int mpp_device_video_force_key(device_t *dev)
{
  if (!dev->mpp || !dev->mpp->mpi || !dev->mpp->ctx)
    return -1;

  if (dev->mpp->codec_type != MPP_CODEC_TYPE_ENCODER)
    return -1;

  MPP_RET ret = dev->mpp->mpi->control(dev->mpp->ctx, MPP_ENC_SET_IDR_FRAME, NULL);
  if (ret != MPP_OK) {
    LOG_INFO(dev, "Failed to force IDR frame: %d", ret);
    return -1;
  }

  LOG_DEBUG(dev, "Forced IDR frame");
  return 0;
}

void mpp_device_dump_options(device_t *dev, FILE *stream)
{
  if (!dev->mpp)
    return;

  fprintf(stream, "MPP Device: %s\n", dev->name);
  fprintf(stream, "  Codec type: %s\n",
    dev->mpp->codec_type == MPP_CODEC_TYPE_DECODER ? "decoder" : "encoder");
  fprintf(stream, "  Resolution: %dx%d\n", dev->mpp->width, dev->mpp->height);
  fprintf(stream, "  Stride: %dx%d\n", dev->mpp->hor_stride, dev->mpp->ver_stride);
  if (dev->mpp->codec_type == MPP_CODEC_TYPE_ENCODER) {
    fprintf(stream, "  Bitrate: %d\n", dev->mpp->bitrate);
    fprintf(stream, "  FPS: %d\n", dev->mpp->fps);
  }
}

int mpp_device_dump_options2(device_t *dev, device_option_fn fn, void *opaque)
{
  return 0;
}

int mpp_device_set_fps(device_t *dev, int desired_fps)
{
  if (!dev->mpp)
    return -1;

  dev->mpp->fps = desired_fps;

  if (dev->mpp->codec_type == MPP_CODEC_TYPE_ENCODER && dev->mpp->mpi && dev->mpp->ctx) {
    MppEncCfg cfg;
    mpp_enc_cfg_init(&cfg);
    dev->mpp->mpi->control(dev->mpp->ctx, MPP_ENC_GET_CFG, cfg);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_in_num", desired_fps);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_in_denorm", 1);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_out_num", desired_fps);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_out_denorm", 1);
    dev->mpp->mpi->control(dev->mpp->ctx, MPP_ENC_SET_CFG, cfg);
    mpp_enc_cfg_deinit(cfg);
  }

  LOG_DEBUG(dev, "Set FPS to %d", desired_fps);
  return 0;
}

int mpp_device_set_option(device_t *dev, const char *key, const char *value)
{
  if (!dev->mpp)
    return -1;

  if (strcmp(key, "bitrate") == 0) {
    dev->mpp->bitrate = atoi(value);
    if (dev->mpp->codec_type == MPP_CODEC_TYPE_ENCODER && dev->mpp->mpi && dev->mpp->ctx) {
      MppEncCfg cfg;
      mpp_enc_cfg_init(&cfg);
      dev->mpp->mpi->control(dev->mpp->ctx, MPP_ENC_GET_CFG, cfg);
      mpp_enc_cfg_set_s32(cfg, "rc:bps_target", dev->mpp->bitrate);
      mpp_enc_cfg_set_s32(cfg, "rc:bps_max", dev->mpp->bitrate * 17 / 16);
      mpp_enc_cfg_set_s32(cfg, "rc:bps_min", dev->mpp->bitrate * 15 / 16);
      dev->mpp->mpi->control(dev->mpp->ctx, MPP_ENC_SET_CFG, cfg);
      mpp_enc_cfg_deinit(cfg);
    }
    LOG_DEBUG(dev, "Set bitrate to %d", dev->mpp->bitrate);
    return 0;
  }

  return -1;
}

static device_hw_t hw_mpp = {
  .device_open = mpp_device_open,
  .device_close = mpp_device_close,
  .device_video_force_key = mpp_device_video_force_key,
  .device_dump_options = mpp_device_dump_options,
  .device_dump_options2 = mpp_device_dump_options2,
  .device_set_fps = mpp_device_set_fps,
  .device_set_option = mpp_device_set_option,
  .buffer_open = mpp_buffer_open,
  .buffer_close = mpp_buffer_close,
  .buffer_enqueue = mpp_buffer_enqueue,
  .buffer_list_dequeue = mpp_buffer_list_dequeue,
  .buffer_list_pollfd = mpp_buffer_list_pollfd,
  .buffer_list_open = mpp_buffer_list_open,
  .buffer_list_close = mpp_buffer_list_close,
  .buffer_list_alloc_buffers = mpp_buffer_list_alloc_buffers,
  .buffer_list_free_buffers = mpp_buffer_list_free_buffers,
  .buffer_list_set_stream = mpp_buffer_list_set_stream,
};

static int mpp_init_decoder(device_t *dev, unsigned input_format, unsigned output_format)
{
  MPP_RET ret;

  dev->mpp->codec_type = MPP_CODEC_TYPE_DECODER;
  dev->mpp->coding_type = v4l2_to_mpp_coding(input_format);
  dev->mpp->frame_fmt = v4l2_to_mpp_format(output_format);
  dev->mpp->input_format = input_format;
  dev->mpp->output_format = output_format;

  if (dev->mpp->coding_type == MPP_VIDEO_CodingUnused) {
    LOG_INFO(dev, "Unsupported input format for decoder");
    return -1;
  }

  ret = mpp_create(&dev->mpp->ctx, &dev->mpp->mpi);
  if (ret != MPP_OK) {
    LOG_INFO(dev, "mpp_create failed: %d", ret);
    return -1;
  }

  ret = mpp_init(dev->mpp->ctx, MPP_CTX_DEC, dev->mpp->coding_type);
  if (ret != MPP_OK) {
    LOG_INFO(dev, "mpp_init decoder failed: %d", ret);
    return -1;
  }

  MppDecCfg cfg = NULL;
  mpp_dec_cfg_init(&cfg);
  mpp_dec_cfg_set_u32(cfg, "base:split_parse", 1);
  ret = dev->mpp->mpi->control(dev->mpp->ctx, MPP_DEC_SET_CFG, cfg);
  mpp_dec_cfg_deinit(cfg);

  if (ret != MPP_OK) {
    LOG_INFO(dev, "Failed to set decoder config: %d", ret);
    return -1;
  }

  dev->mpp->initialized = true;
  LOG_INFO(dev, "MPP decoder initialized");
  return 0;
}

static int mpp_init_encoder(device_t *dev, unsigned input_format, unsigned output_format)
{
  MPP_RET ret;

  dev->mpp->codec_type = MPP_CODEC_TYPE_ENCODER;
  dev->mpp->coding_type = v4l2_to_mpp_coding(output_format);
  dev->mpp->frame_fmt = v4l2_to_mpp_format(input_format);
  dev->mpp->input_format = input_format;
  dev->mpp->output_format = output_format;

  if (dev->mpp->coding_type == MPP_VIDEO_CodingUnused) {
    LOG_INFO(dev, "Unsupported output format for encoder");
    return -1;
  }

  ret = mpp_create(&dev->mpp->ctx, &dev->mpp->mpi);
  if (ret != MPP_OK) {
    LOG_INFO(dev, "mpp_create failed: %d", ret);
    return -1;
  }

  ret = mpp_init(dev->mpp->ctx, MPP_CTX_ENC, dev->mpp->coding_type);
  if (ret != MPP_OK) {
    LOG_INFO(dev, "mpp_init encoder failed: %d", ret);
    return -1;
  }

  dev->mpp->initialized = true;
  LOG_INFO(dev, "MPP encoder initialized (config will be set on buffer list open)");
  return 0;
}

device_t *device_mpp_decoder_open(const char *name, unsigned input_format, unsigned output_format)
{
  char path[64];
  snprintf(path, sizeof(path), "mpp://decoder/%s", name);

  device_t *dev = device_open(name, path, &hw_mpp);
  if (!dev)
    return NULL;

  if (mpp_init_decoder(dev, input_format, output_format) < 0) {
    device_close(dev);
    return NULL;
  }

  return dev;
}

device_t *device_mpp_encoder_open(const char *name, unsigned input_format, unsigned output_format)
{
  char path[64];
  snprintf(path, sizeof(path), "mpp://encoder/%s", name);

  device_t *dev = device_open(name, path, &hw_mpp);
  if (!dev)
    return NULL;

  if (mpp_init_encoder(dev, input_format, output_format) < 0) {
    device_close(dev);
    return NULL;
  }

  return dev;
}

#endif
