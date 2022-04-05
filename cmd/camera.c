#include "camera.h"

#include "hw/device.h"
#include "hw/links.h"
#include "hw/v4l2.h"

void camera_init(camera_t *camera)
{
  memset(camera, 0, sizeof(*camera));
  camera->width = 1280;
  camera->height = 720;
  camera->nbufs = 4;
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

int camera_open(camera_t *camera, const char *path)
{
  camera->camera = device_open("CAMERA", path);
  if (!camera->camera) {
    return -1;
  }

  return 0;;
}

int camera_set_params(camera_t *camera)
{
  DEVICE_SET_OPTION(camera->camera, EXPOSURE, 1148);
  DEVICE_SET_OPTION(camera->camera, ANALOGUE_GAIN, 938);
  DEVICE_SET_OPTION(camera->camera, DIGITAL_GAIN, 256);

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