#include "camera.h"

#include "hw/device.h"
#include "hw/buffer_list.h"
#include "hw/links.h"
#include "hw/v4l2.h"

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

  memset(camera->links, 0, sizeof(camera->links));
  free(camera);
}

camera_t *camera_open(camera_options_t *options)
{
  camera_t *camera = calloc(1, sizeof(camera_t));
  camera->name = "CAMERA";
  camera->options = *options;

  camera->camera = device_open(camera->name, camera->options.path);
  if (!camera->camera) {
    goto error;
  }

  camera->camera->allow_dma = camera->options.allow_dma;

  if (strstr(camera->camera->v4l2_cap.bus_info, "usb")) {
    E_LOG_INFO(camera, "Disabling DMA since device uses USB (which is likely not working properly).");
    camera->camera->allow_dma = false;
  }

  device_set_pad_format(camera->camera, camera->options.width, camera->options.height, 0);

  if (device_open_buffer_list(camera->camera, true, camera->options.width, camera->options.height, camera->options.format, 0, camera->options.nbufs, true) < 0) {
    goto error;
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
    if (camera_configure_legacy_isp(&camera, camera->options.high_res_factor) < 0) {
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

  DEVICE_SET_OPTION(camera->camera, EXPOSURE, 2684);
  DEVICE_SET_OPTION(camera->camera, ANALOGUE_GAIN, 938);
  DEVICE_SET_OPTION(camera->camera, DIGITAL_GAIN, 512);
  DEVICE_SET_OPTION(camera->camera, VBLANK, 1636);
  DEVICE_SET_OPTION(camera->camera, HBLANK, 6906);

  DEVICE_SET_OPTION2(camera->codec_jpeg, JPEG, COMPRESSION_QUALITY, 80);

  DEVICE_SET_OPTION2(camera->codec_h264, MPEG_VIDEO, BITRATE, 5000 * 1000);
  DEVICE_SET_OPTION2(camera->codec_h264, MPEG_VIDEO, H264_I_PERIOD, 30);
  DEVICE_SET_OPTION2(camera->codec_h264, MPEG_VIDEO, H264_LEVEL, V4L2_MPEG_VIDEO_H264_LEVEL_4_0);
  DEVICE_SET_OPTION2(camera->codec_h264, MPEG_VIDEO, REPEAT_SEQ_HEADER, 1);
  DEVICE_SET_OPTION2(camera->codec_h264, MPEG_VIDEO, H264_MIN_QP, 16);
  DEVICE_SET_OPTION2(camera->codec_h264, MPEG_VIDEO, H264_MIN_QP, 32);
  return 0;
}

int camera_run(camera_t *camera)
{
  bool running = false;
  return links_loop(camera->links, &running);
}