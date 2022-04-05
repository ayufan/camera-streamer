#include "camera.h"

#include "hw/device.h"
#include "hw/links.h"
#include "hw/v4l2.h"

void camera_init(camera_t *camera)
{
  memset(camera, 0, sizeof(*camera));
  camera->name = "CAMERA";
  strcpy(camera->path, "/dev/video0");
  camera->width = 1280;
  camera->height = 720;
  camera->nbufs = 4;
  camera->format = V4L2_PIX_FMT_SRGGB10P;
  camera->allow_dma = true;
  camera->fps = 30;
}

void camera_close(camera_t *camera)
{
  for (int i = MAX_DEVICES; i-- > 0; ) {
    if (camera->devices[i]) {
      device_close(camera->devices[i]);
      camera->devices[i] = NULL;
    }
  }

  memset(camera->links, 0, sizeof(camera->links));
}

int camera_open(camera_t *camera)
{
  camera->camera = device_open("CAMERA", camera->path);
  if (!camera->camera) {
    return -1;
  }

  camera->camera->allow_dma = camera->allow_dma;

  switch (camera->format) {
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
    if (camera_configure_isp(camera, 1, 0) < 0) {
      goto error;
    }
#else
    if (camera_configure_legacy_isp(&camera, 1.3) < 0) {
      goto error;
    }
#endif
    break;

  default:
    E_LOG_ERROR(camera, "Unsupported camera format=%s", fourcc_to_string(camera->format).buf);
    break;
  }

  return 0;

error:
  return -1;
}

int camera_set_params(camera_t *camera)
{
  device_set_fps(camera->camera, camera->fps);

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