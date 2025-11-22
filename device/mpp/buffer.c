#ifdef USE_MPP

#include "mpp.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "util/opts/log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>

#ifndef MPP_PACKET_FLAG_INTRA
#define MPP_PACKET_FLAG_INTRA           (0x00000010)
#endif

int mpp_buffer_open(buffer_t *buf)
{
  buffer_list_t *buf_list = buf->buf_list;

  buf->mpp = calloc(1, sizeof(buffer_mpp_t));
  if (!buf->mpp) {
    LOG_ERROR(buf, "Failed to allocate MPP buffer structure");
    goto error;
  }

  if (buf_list->do_mmap) {
    size_t buf_size = buf_list->fmt.sizeimage;
    if (buf_size == 0) {
      buf_size = buf_list->fmt.width * buf_list->fmt.height * 3 / 2;
    }

    MPP_RET ret = mpp_buffer_get(buf_list->mpp->buf_grp, &buf->mpp->mpp_buf, buf_size);
    if (ret != MPP_OK) {
      LOG_ERROR(buf, "Failed to get MPP buffer: %d", ret);
      goto error;
    }

    buf->start = mpp_buffer_get_ptr(buf->mpp->mpp_buf);
    buf->length = mpp_buffer_get_size(buf->mpp->mpp_buf);
    buf->dma_fd = mpp_buffer_get_fd(buf->mpp->mpp_buf);

    LOG_DEBUG(buf, "MPP buffer allocated: ptr=%p, size=%zu, fd=%d",
      buf->start, buf->length, buf->dma_fd);
  } else {
    buf->start = NULL;
    buf->length = 0;
    buf->dma_fd = -1;
  }

  return 0;

error:
  mpp_buffer_close(buf);
  return -1;
}

void mpp_buffer_close(buffer_t *buf)
{
  if (!buf->mpp)
    return;

  if (buf->mpp->frame) {
    mpp_frame_deinit(&buf->mpp->frame);
    buf->mpp->frame = NULL;
  }

  if (buf->mpp->packet) {
    mpp_packet_deinit(&buf->mpp->packet);
    buf->mpp->packet = NULL;
  }

  if (buf->mpp->mpp_buf) {
    mpp_buffer_put(buf->mpp->mpp_buf);
    buf->mpp->mpp_buf = NULL;
  }

  buf->start = NULL;
  buf->dma_fd = -1;

  free(buf->mpp);
  buf->mpp = NULL;
}

int mpp_buffer_enqueue(buffer_t *buf, const char *who)
{
  buffer_list_t *buf_list = buf->buf_list;
  device_t *dev = buf_list->dev;
  MPP_RET ret;

  if (!dev->mpp || !dev->mpp->mpi || !dev->mpp->ctx) {
    LOG_ERROR(buf, "MPP device not initialized");
    return -1;
  }

  if (dev->mpp->codec_type == MPP_CODEC_TYPE_DECODER) {
    if (!buf_list->do_capture) {
      MppPacket packet = NULL;
      void *data = buf->dma_source ? buf->dma_source->start : buf->start;
      size_t size = buf->dma_source ? buf->dma_source->used : buf->used;

      if (!data || size == 0) {
        LOG_ERROR(buf, "No data to decode");
        return -1;
      }

      ret = mpp_packet_init(&packet, data, size);
      if (ret != MPP_OK) {
        LOG_ERROR(buf, "mpp_packet_init failed: %d", ret);
        return -1;
      }

      mpp_packet_set_pts(packet, buf->captured_time_us);
      mpp_packet_set_pos(packet, data);
      mpp_packet_set_length(packet, size);

      do {
        ret = dev->mpp->mpi->decode_put_packet(dev->mpp->ctx, packet);
        if (ret == MPP_ERR_BUFFER_FULL) {
          usleep(1000);
        }
      } while (ret == MPP_ERR_BUFFER_FULL);

      mpp_packet_deinit(&packet);

      if (ret != MPP_OK) {
        LOG_ERROR(buf, "decode_put_packet failed: %d", ret);
        return -1;
      }
    }
  } else if (dev->mpp->codec_type == MPP_CODEC_TYPE_ENCODER) {
    if (!buf_list->do_capture) {
      MppFrame frame = NULL;
      void *data = buf->dma_source ? buf->dma_source->start : buf->start;
      size_t size = buf->dma_source ? buf->dma_source->used : buf->used;
      int dma_fd = buf->dma_source ? buf->dma_source->dma_fd : buf->dma_fd;

      ret = mpp_frame_init(&frame);
      if (ret != MPP_OK) {
        LOG_ERROR(buf, "mpp_frame_init failed: %d", ret);
        return -1;
      }

      mpp_frame_set_width(frame, dev->mpp->width);
      mpp_frame_set_height(frame, dev->mpp->height);
      mpp_frame_set_hor_stride(frame, dev->mpp->hor_stride);
      mpp_frame_set_ver_stride(frame, dev->mpp->ver_stride);
      mpp_frame_set_fmt(frame, dev->mpp->frame_fmt);
      mpp_frame_set_pts(frame, buf->captured_time_us);

      if (dma_fd >= 0 && dev->opts.allow_dma) {
        MppBuffer frame_buf = NULL;
        MppBufferInfo info = {
          .type = MPP_BUFFER_TYPE_DRM,
          .fd = dma_fd,
          .size = size,
          .ptr = data,
        };

        ret = mpp_buffer_import(&frame_buf, &info);
        if (ret == MPP_OK) {
          mpp_frame_set_buffer(frame, frame_buf);
          mpp_buffer_put(frame_buf);
        }
      }

      if (!mpp_frame_get_buffer(frame) && data) {
        MppBuffer frame_buf = NULL;
        ret = mpp_buffer_get(dev->mpp->buf_grp, &frame_buf, size);
        if (ret == MPP_OK) {
          memcpy(mpp_buffer_get_ptr(frame_buf), data, size);
          mpp_frame_set_buffer(frame, frame_buf);
          mpp_buffer_put(frame_buf);
        }
      }

      if (buf->flags.is_keyframe) {
        dev->mpp->mpi->control(dev->mpp->ctx, MPP_ENC_SET_IDR_FRAME, NULL);
      }

      do {
        ret = dev->mpp->mpi->encode_put_frame(dev->mpp->ctx, frame);
        if (ret == MPP_ERR_BUFFER_FULL) {
          usleep(1000);
        }
      } while (ret == MPP_ERR_BUFFER_FULL);

      mpp_frame_deinit(&frame);

      if (ret != MPP_OK) {
        LOG_ERROR(buf, "encode_put_frame failed: %d", ret);
        return -1;
      }
    }
  }

  return 0;

error:
  return -1;
}

int mpp_buffer_list_dequeue(buffer_list_t *buf_list, buffer_t **bufp)
{
  device_t *dev = buf_list->dev;
  MPP_RET ret;

  if (!dev->mpp || !dev->mpp->mpi || !dev->mpp->ctx) {
    return -1;
  }

  *bufp = NULL;

  if (dev->mpp->codec_type == MPP_CODEC_TYPE_DECODER) {
    if (buf_list->do_capture) {
      MppFrame frame = NULL;

      ret = dev->mpp->mpi->decode_get_frame(dev->mpp->ctx, &frame);
      if (ret != MPP_OK || !frame) {
        return -1;
      }

      if (mpp_frame_get_info_change(frame)) {
        RK_U32 width = mpp_frame_get_width(frame);
        RK_U32 height = mpp_frame_get_height(frame);
        RK_U32 hor_stride = mpp_frame_get_hor_stride(frame);
        RK_U32 ver_stride = mpp_frame_get_ver_stride(frame);
        MppFrameFormat fmt = mpp_frame_get_fmt(frame);

        LOG_INFO(buf_list, "Decoder info change: %dx%d stride %dx%d fmt %d",
          width, height, hor_stride, ver_stride, fmt);

        dev->mpp->width = width;
        dev->mpp->height = height;
        dev->mpp->hor_stride = hor_stride;
        dev->mpp->ver_stride = ver_stride;
        dev->mpp->frame_fmt = fmt;

        dev->mpp->mpi->control(dev->mpp->ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
        mpp_frame_deinit(&frame);
        return -1;
      }

      if (mpp_frame_get_errinfo(frame) || mpp_frame_get_discard(frame)) {
        mpp_frame_deinit(&frame);
        return -1;
      }

      buffer_t *buf = buffer_list_find_slot(buf_list);
      if (!buf) {
        mpp_frame_deinit(&frame);
        return -1;
      }

      MppBuffer mpp_buf = mpp_frame_get_buffer(frame);
      if (mpp_buf) {
        buf->start = mpp_buffer_get_ptr(mpp_buf);
        buf->length = mpp_buffer_get_size(mpp_buf);
        buf->used = dev->mpp->hor_stride * dev->mpp->ver_stride * 3 / 2;
        buf->dma_fd = mpp_buffer_get_fd(mpp_buf);
      }

      buf->captured_time_us = mpp_frame_get_pts(frame);
      buf->flags.is_keyframe = true;

      if (buf->mpp->frame) {
        mpp_frame_deinit(&buf->mpp->frame);
      }
      buf->mpp->frame = frame;

      *bufp = buf;
      return 0;
    }
  } else if (dev->mpp->codec_type == MPP_CODEC_TYPE_ENCODER) {
    if (buf_list->do_capture) {
      MppPacket packet = NULL;

      ret = dev->mpp->mpi->encode_get_packet(dev->mpp->ctx, &packet);
      if (ret != MPP_OK || !packet) {
        return -1;
      }

      buffer_t *buf = buffer_list_find_slot(buf_list);
      if (!buf) {
        mpp_packet_deinit(&packet);
        return -1;
      }

      void *data = mpp_packet_get_pos(packet);
      size_t len = mpp_packet_get_length(packet);

      if (buf->mpp->mpp_buf) {
        if (len <= buf->length) {
          memcpy(buf->start, data, len);
          buf->used = len;
        } else {
          LOG_ERROR(buf_list, "Encoded packet too large: %zu > %zu", len, buf->length);
          mpp_packet_deinit(&packet);
          return -1;
        }
      } else {
        buf->start = data;
        buf->used = len;
        buf->length = len;
      }

      buf->captured_time_us = mpp_packet_get_pts(packet);
      buf->flags.is_keyframe = (mpp_packet_get_flag(packet) & MPP_PACKET_FLAG_INTRA) != 0;

      if (buf->mpp->packet) {
        mpp_packet_deinit(&buf->mpp->packet);
      }
      buf->mpp->packet = packet;

      *bufp = buf;
      return 0;
    }
  }

error:
  return -1;
}

int mpp_buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue)
{
  pollfd->fd = -1;
  pollfd->events = 0;
  pollfd->revents = 0;

  if (can_dequeue && buffer_list_count_enqueued(buf_list) > 0) {
    pollfd->revents = buf_list->do_capture ? POLLIN : POLLOUT;
  }

  return 0;
}

#endif
