#ifdef USE_MPP

#include "mpp.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"

#include <stdlib.h>
#include <string.h>

static int mpp_align(int val, int align)
{
  return (val + align - 1) & ~(align - 1);
}

int mpp_buffer_list_open(buffer_list_t *buf_list)
{
  device_t *dev = buf_list->dev;
  MPP_RET ret;

  buf_list->mpp = calloc(1, sizeof(buffer_list_mpp_t));
  if (!buf_list->mpp) {
    LOG_ERROR(buf_list, "Failed to allocate MPP buffer list structure");
    return -1;
  }

  buf_list->mpp->do_capture = buf_list->do_capture;

  buffer_format_t fmt = buf_list->fmt;

  if (!buf_list->do_capture) {
    dev->mpp->width = fmt.width;
    dev->mpp->height = fmt.height;
    dev->mpp->hor_stride = mpp_align(fmt.width, 16);
    dev->mpp->ver_stride = mpp_align(fmt.height, 16);

    if (dev->mpp->codec_type == MPP_CODEC_TYPE_ENCODER) {
      MppEncCfg cfg;
      ret = mpp_enc_cfg_init(&cfg);
      if (ret != MPP_OK) {
        LOG_ERROR(buf_list, "mpp_enc_cfg_init failed: %d", ret);
        goto error;
      }

      mpp_enc_cfg_set_s32(cfg, "prep:width", dev->mpp->width);
      mpp_enc_cfg_set_s32(cfg, "prep:height", dev->mpp->height);
      mpp_enc_cfg_set_s32(cfg, "prep:hor_stride", dev->mpp->hor_stride);
      mpp_enc_cfg_set_s32(cfg, "prep:ver_stride", dev->mpp->ver_stride);
      mpp_enc_cfg_set_s32(cfg, "prep:format", dev->mpp->frame_fmt);

      mpp_enc_cfg_set_s32(cfg, "rc:mode", MPP_ENC_RC_MODE_CBR);
      mpp_enc_cfg_set_s32(cfg, "rc:fps_in_flex", 0);
      mpp_enc_cfg_set_s32(cfg, "rc:fps_in_num", dev->mpp->fps);
      mpp_enc_cfg_set_s32(cfg, "rc:fps_in_denorm", 1);
      mpp_enc_cfg_set_s32(cfg, "rc:fps_out_flex", 0);
      mpp_enc_cfg_set_s32(cfg, "rc:fps_out_num", dev->mpp->fps);
      mpp_enc_cfg_set_s32(cfg, "rc:fps_out_denorm", 1);
      mpp_enc_cfg_set_s32(cfg, "rc:gop", dev->mpp->fps * 2);
      mpp_enc_cfg_set_s32(cfg, "rc:bps_target", dev->mpp->bitrate);
      mpp_enc_cfg_set_s32(cfg, "rc:bps_max", dev->mpp->bitrate * 17 / 16);
      mpp_enc_cfg_set_s32(cfg, "rc:bps_min", dev->mpp->bitrate * 15 / 16);

      if (dev->mpp->coding_type == MPP_VIDEO_CodingAVC) {
        mpp_enc_cfg_set_s32(cfg, "codec:type", MPP_VIDEO_CodingAVC);
        mpp_enc_cfg_set_s32(cfg, "h264:profile", 100);
        mpp_enc_cfg_set_s32(cfg, "h264:level", 41);
        mpp_enc_cfg_set_s32(cfg, "h264:cabac_en", 1);
        mpp_enc_cfg_set_s32(cfg, "h264:cabac_idc", 0);
        mpp_enc_cfg_set_s32(cfg, "h264:trans8x8", 1);
        mpp_enc_cfg_set_s32(cfg, "h264:qp_init", 26);
        mpp_enc_cfg_set_s32(cfg, "h264:qp_min", 10);
        mpp_enc_cfg_set_s32(cfg, "h264:qp_max", 51);
        mpp_enc_cfg_set_s32(cfg, "h264:qp_min_i", 10);
        mpp_enc_cfg_set_s32(cfg, "h264:qp_max_i", 51);
      } else if (dev->mpp->coding_type == MPP_VIDEO_CodingMJPEG) {
        mpp_enc_cfg_set_s32(cfg, "codec:type", MPP_VIDEO_CodingMJPEG);
        mpp_enc_cfg_set_s32(cfg, "jpeg:quant", 7);
      }

      ret = dev->mpp->mpi->control(dev->mpp->ctx, MPP_ENC_SET_CFG, cfg);
      mpp_enc_cfg_deinit(cfg);

      if (ret != MPP_OK) {
        LOG_ERROR(buf_list, "Failed to set encoder config: %d", ret);
        goto error;
      }

      if (dev->mpp->coding_type == MPP_VIDEO_CodingAVC) {
        MppEncHeaderMode header_mode = MPP_ENC_HEADER_MODE_EACH_IDR;
        ret = dev->mpp->mpi->control(dev->mpp->ctx, MPP_ENC_SET_HEADER_MODE, &header_mode);
        if (ret != MPP_OK) {
          LOG_VERBOSE(buf_list, "Failed to set header mode: %d", ret);
        }
      }

      LOG_INFO(buf_list, "MPP encoder configured: %dx%d stride %dx%d",
        dev->mpp->width, dev->mpp->height, dev->mpp->hor_stride, dev->mpp->ver_stride);
    }
  }

  if (buf_list->do_capture) {
    if (dev->mpp->codec_type == MPP_CODEC_TYPE_DECODER) {
      buf_list->fmt.format = mpp_to_v4l2_format(dev->mpp->frame_fmt);
    } else {
      buf_list->fmt.format = dev->mpp->output_format;
    }
  } else {
    if (dev->mpp->codec_type == MPP_CODEC_TYPE_DECODER) {
      buf_list->fmt.format = dev->mpp->input_format;
    } else {
      buf_list->fmt.format = mpp_to_v4l2_format(dev->mpp->frame_fmt);
    }
  }

  buf_list->fmt.width = dev->mpp->width;
  buf_list->fmt.height = dev->mpp->height;
  buf_list->fmt.bytesperline = dev->mpp->hor_stride;

  if (buf_list->fmt.sizeimage == 0) {
    if (buf_list->do_capture && dev->mpp->codec_type == MPP_CODEC_TYPE_ENCODER) {
      buf_list->fmt.sizeimage = dev->mpp->width * dev->mpp->height;
    } else {
      buf_list->fmt.sizeimage = dev->mpp->hor_stride * dev->mpp->ver_stride * 3 / 2;
    }
  }

  LOG_INFO(buf_list, "MPP buffer list opened: %s format=%s %dx%d",
    buf_list->do_capture ? "capture" : "output",
    fourcc_to_string(buf_list->fmt.format).buf,
    buf_list->fmt.width, buf_list->fmt.height);

  return buf_list->fmt.nbufs;

error:
  mpp_buffer_list_close(buf_list);
  return -1;
}

void mpp_buffer_list_close(buffer_list_t *buf_list)
{
  if (!buf_list->mpp)
    return;

  if (buf_list->mpp->buf_grp) {
    mpp_buffer_group_put(buf_list->mpp->buf_grp);
    buf_list->mpp->buf_grp = NULL;
  }

  free(buf_list->mpp);
  buf_list->mpp = NULL;

  LOG_DEBUG(buf_list, "MPP buffer list closed");
}

int mpp_buffer_list_alloc_buffers(buffer_list_t *buf_list)
{
  if (!buf_list->mpp)
    return -1;

  if (buf_list->do_mmap) {
    MPP_RET ret = mpp_buffer_group_get_internal(&buf_list->mpp->buf_grp, MPP_BUFFER_TYPE_DRM);
    if (ret != MPP_OK) {
      LOG_ERROR(buf_list, "Failed to create buffer group: %d", ret);
      return -1;
    }

    device_t *dev = buf_list->dev;
    if (buf_list->do_capture && dev->mpp->codec_type == MPP_CODEC_TYPE_DECODER) {
      dev->mpp->mpi->control(dev->mpp->ctx, MPP_DEC_SET_EXT_BUF_GROUP, buf_list->mpp->buf_grp);
    }
  }

  LOG_DEBUG(buf_list, "MPP buffer group allocated");
  return 0;

error:
  return -1;
}

void mpp_buffer_list_free_buffers(buffer_list_t *buf_list)
{
  if (!buf_list->mpp)
    return;

  if (buf_list->mpp->buf_grp) {
    mpp_buffer_group_clear(buf_list->mpp->buf_grp);
  }

  LOG_DEBUG(buf_list, "MPP buffers freed");
}

int mpp_buffer_list_set_stream(buffer_list_t *buf_list, bool do_on)
{
  device_t *dev = buf_list->dev;

  if (!dev->mpp)
    return -1;

  if (!do_on) {
    if (dev->mpp->mpi && dev->mpp->ctx) {
      dev->mpp->mpi->reset(dev->mpp->ctx);
    }

    for (int i = 0; i < buf_list->nbufs; i++) {
      buffer_t *buf = buf_list->bufs[i];
      if (!buf->enqueued)
        continue;

      if (buf->dma_source) {
        buf->dma_source->used = 0;
        buffer_consumed(buf->dma_source, "stream-off");
        buf->dma_source = NULL;
      }

      buf->enqueued = false;
      buf->mmap_reflinks = 1;
    }
  }

  LOG_DEBUG(buf_list, "MPP stream %s", do_on ? "on" : "off");
  return 0;
}

#endif
