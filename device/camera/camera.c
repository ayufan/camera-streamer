#include "camera.h"

#include "device/device.h"
#include "device/buffer_list.h"
#include "device/links.h"
#include "device/hw/v4l2.h"

void camera_init(camera_t *camera)
{
  memset(camera, 0, sizeof(*camera));
  camera->name = "CAMERA";
}

void camera_close(camera_t *camera)
{
  if (!camera)
    return;

  for (int i = MAX_DEVICES; i-- > 0; ) {
    if (camera->devices[i]) {
      device_close(camera->devices[i]);
      camera->devices[i] = NULL;
    }
  }

  for (int i = MAX_DEVICES; i-- > 0; ) {
    if (camera->links[i].callbacks.on_buffer) {
      camera->links[i].callbacks.on_buffer = NULL;
    }
  }

  memset(camera->links, 0, sizeof(camera->links));
  free(camera);
}

camera_t *camera_open(camera_options_t *options)
{
  camera_t *camera = calloc(1, sizeof(camera_t));
  camera->name = "CAMERA";
  camera->options = *options;

  camera->camera = device_v4l2_open(camera->name, camera->options.path);
  if (!camera->camera) {
    goto error;
  }

  camera->camera->allow_dma = camera->options.allow_dma;

  if (strstr(camera->camera->bus_info, "usb")) {
    E_LOG_INFO(camera, "Disabling DMA since device uses USB (which is likely not working properly).");
    camera->camera->allow_dma = false;
  }

  // TODO: mpad format
  // device_set_pad_format(camera->camera, camera->options.width, camera->options.height, 0);

  if (device_open_buffer_list(camera->camera, true, camera->options.width, camera->options.height, camera->options.format, 0, camera->options.nbufs, true) < 0) {
    goto error;
  }
  camera->camera->capture_list->do_timestamps = true;

  if (camera->options.fps > 0) {
    camera->camera->capture_list->fmt_interval_us = 1000 * 1000 / camera->options.fps;
  }

  switch (camera->camera->capture_list->fmt_format) {
  case V4L2_PIX_FMT_YUYV:
    if (camera_configure_direct(camera) < 0) {
      goto error;
    }
    break;

  case V4L2_PIX_FMT_MJPEG:
  case V4L2_PIX_FMT_H264:
    if (camera_configure_decoder(camera) < 0) {
      goto error;
    }
    break;

  case V4L2_PIX_FMT_SRGGB10P:
#if 1
    if (camera_configure_isp(camera, camera->options.high_res_factor, camera->options.low_res_factor) < 0) {
      goto error;
    }
#else
    if (camera_configure_legacy_isp(camera, camera->options.high_res_factor) < 0) {
      goto error;
    }
#endif
    break;

  default:
    E_LOG_ERROR(camera, "Unsupported camera format=%s", fourcc_to_string(camera->options.format).buf);
    break;
  }

  if (camera_set_params(camera) < 0) {
    goto error;
  }

  return camera;

error:
  camera_close(camera);
  return NULL;
}

int camera_set_params(camera_t *camera)
{
  device_set_fps(camera->camera, camera->options.fps);
  device_set_option_list(camera->camera, camera->options.options);
  device_set_option_list(camera->isp_srgb, camera->options.isp.options);

  // DEVICE_SET_OPTION(camera->camera, EXPOSURE, 2684);
  // DEVICE_SET_OPTION(camera->camera, ANALOGUE_GAIN, 938);
  // DEVICE_SET_OPTION(camera->camera, DIGITAL_GAIN, 512);
  // DEVICE_SET_OPTION(camera->camera, VBLANK, 1636);
  // DEVICE_SET_OPTION(camera->camera, HBLANK, 6906);

  // DEVICE_SET_OPTION(camera->isp_srgb, RED_BALANCE, 2120);
  // DEVICE_SET_OPTION(camera->isp_srgb, BLUE_BALANCE, 1472);
  // DEVICE_SET_OPTION(camera->isp_srgb, DIGITAL_GAIN, 1007);

  device_set_option_string(camera->codec_jpeg, "compression_quality", "80");

  device_set_option_string(camera->codec_h264, "video_bitrate_mode", "0");
  device_set_option_string(camera->codec_h264, "video_bitrate", "5000000");
  device_set_option_string(camera->codec_h264, "repeat_sequence_header", "1");
  device_set_option_string(camera->codec_h264, "h264_i_frame_period", "30");
  device_set_option_string(camera->codec_h264, "h264_level", "11");
  device_set_option_string(camera->codec_h264, "h264_profile", "4");
  device_set_option_string(camera->codec_h264, "h264_minimum_qp_value", "16");
  device_set_option_string(camera->codec_h264, "h264_maximum_qp_value", "32");
  return 0;
}

int camera_run(camera_t *camera)
{
  bool running = false;
  return links_loop(camera->links, &running);
}