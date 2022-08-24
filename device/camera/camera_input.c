#include "camera.h"

#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/links.h"
#include "opts/log.h"
#include "opts/fourcc.h"

static int camera_configure_input_v4l2(camera_t *camera)
{
  const char *path = camera->options.path;

  if (!*path) {
    path = "/dev/video0";
  }

  camera->camera = device_v4l2_open(camera->name, path);
  if (!camera->camera) {
    LOG_INFO(camera, "Listing available v4l2 devices:");
    system("v4l2-ctl --list-devices");
    return -1;
  }

  device_set_rotation(camera->camera, camera->options.vflip, camera->options.hflip);

  camera->camera->opts.allow_dma = camera->options.allow_dma;

  if (strstr(camera->camera->bus_info, "usb")) {
    LOG_INFO(camera, "Disabling DMA since device uses USB (which is likely not working properly).");
    camera->camera->opts.allow_dma = false;
  }

  buffer_list_t *camera_capture = device_open_buffer_list(camera->camera, true, camera->options.width, camera->options.height, camera->options.format, 0, camera->options.nbufs, true);
  if (!camera_capture) {
    return -1;
  }
  camera_capture->do_timestamps = true;

  if (camera->options.fps > 0) {
    camera_capture->fmt.interval_us = 1000 * 1000 / camera->options.fps;
  }

  switch (camera_capture->fmt.format) {
  case V4L2_PIX_FMT_YUYV:
  case V4L2_PIX_FMT_YVYU:
  case V4L2_PIX_FMT_VYUY:
  case V4L2_PIX_FMT_UYVY:
  case V4L2_PIX_FMT_YUV420:
  case V4L2_PIX_FMT_RGB565:
  case V4L2_PIX_FMT_RGB24:
    if (camera->options.high_res_factor > 1) {
      // Use ISP, as there are two resolutions
      return camera_configure_isp(camera, camera_capture,
        camera->options.high_res_factor, camera->options.low_res_factor);
    } else {
      // Use direct approach, as there's likely low frequently used low resolution
      return camera_configure_output_rescaler(camera, camera_capture,
        camera->options.high_res_factor, camera->options.low_res_factor);
    }

  case V4L2_PIX_FMT_MJPEG:
  case V4L2_PIX_FMT_H264:
    return camera_configure_decoder(camera, camera_capture);

  case V4L2_PIX_FMT_SRGGB10P:
    return camera_configure_isp(camera, camera_capture,
      camera->options.high_res_factor, camera->options.low_res_factor);

  default:
    LOG_INFO(camera, "Unsupported camera format=%s",
      fourcc_to_string(camera_capture->fmt.format).buf);
    return -1;
  }
}

static int camera_configure_input_libcamera(camera_t *camera)
{
  camera->camera = device_libcamera_open(camera->name, camera->options.path);
  if (!camera->camera) {
    return -1;
  }

  device_set_rotation(camera->camera, camera->options.vflip, camera->options.hflip);

  camera->camera->opts.allow_dma = camera->options.allow_dma;

  buffer_list_t *camera_capture = device_open_buffer_list(
    camera->camera,
    true,
    camera->options.width / camera->options.high_res_factor,
    camera->options.height / camera->options.high_res_factor,
    camera->options.format,
    0,
    camera->options.nbufs,
    true
  );
  if (!camera_capture) {
    return -1;
  }
  camera_capture->do_timestamps = true;

  if (camera->options.fps > 0) {
    camera_capture->fmt.interval_us = 1000 * 1000 / camera->options.fps;
  }

  return camera_configure_output_rescaler(camera, camera_capture,
    1.0, camera->options.low_res_factor / camera->options.high_res_factor);
}

int camera_configure_input(camera_t *camera)
{
  switch (camera->options.type) {
  case CAMERA_V4L2:
    return camera_configure_input_v4l2(camera);

  case CAMERA_LIBCAMERA:
    return camera_configure_input_libcamera(camera);

  default:
    LOG_INFO(camera, "Unsupported camera type");
    return -1;
  }
}
