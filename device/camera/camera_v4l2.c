#include "camera.h"

#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/links.h"
#include "opts/log.h"
#include "opts/fourcc.h"

int camera_configure_v4l2(camera_t *camera)
{
  camera->camera = device_v4l2_open(camera->name, camera->options.path);
  if (!camera->camera) {
    goto error;
  }

  camera->camera->opts.allow_dma = camera->options.allow_dma;

  if (strstr(camera->camera->bus_info, "usb")) {
    LOG_INFO(camera, "Disabling DMA since device uses USB (which is likely not working properly).");
    camera->camera->opts.allow_dma = false;
  }

  buffer_list_t *src = device_open_buffer_list(camera->camera, true, camera->options.width, camera->options.height, camera->options.format, 0, camera->options.nbufs, true);
  if (!src) {
    goto error;
  }
  src->do_timestamps = true;

  if (camera->options.fps > 0) {
    src->fmt.interval_us = 1000 * 1000 / camera->options.fps;
  }

  switch (src->fmt.format) {
  case V4L2_PIX_FMT_YUYV:
  case V4L2_PIX_FMT_YVYU:
  case V4L2_PIX_FMT_VYUY:
  case V4L2_PIX_FMT_UYVY:
  case V4L2_PIX_FMT_YUV420:
  case V4L2_PIX_FMT_RGB565:
  case V4L2_PIX_FMT_RGB24:
    if (camera_configure_direct(camera, src) < 0) {
      goto error;
    }
    break;

  case V4L2_PIX_FMT_MJPEG:
  case V4L2_PIX_FMT_H264:
    if (camera_configure_decoder(camera, src) < 0) {
      goto error;
    }
    break;

  case V4L2_PIX_FMT_SRGGB10P:
#if 1
    if (camera_configure_isp(camera, src, camera->options.high_res_factor, camera->options.low_res_factor) < 0) {
      goto error;
    }
#else
    if (camera_configure_legacy_isp(camera, src, camera->options.high_res_factor) < 0) {
      goto error;
    }
#endif
    break;

  default:
    LOG_ERROR(camera, "Unsupported camera format=%s",
      fourcc_to_string(src->fmt.format).buf);
    break;
  }

  return 0;

error:
  return -1;
}