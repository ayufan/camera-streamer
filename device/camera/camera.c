#include "camera.h"

#include "device/device.h"
#include "device/buffer_list.h"
#include "device/links.h"
#include "opts/log.h"
#include "opts/fourcc.h"

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

  switch (camera->options.type) {
  case CAMERA_V4L2:
    if (camera_configure_v4l2(camera) < 0) {
      goto error;
    }
    break;

  case CAMERA_LIBCAMERA:
    if (camera_configure_libcamera(camera) < 0) {
      goto error;
    }
    break;

  default:
    LOG_ERROR(camera, "Unsupported camera type");
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
  device_set_option_list(camera->isp, camera->options.isp.options);

  // Set some defaults
  for (int i = 0; i < 2; i++) {
    device_set_option_string(camera->codec_jpeg[i], "compression_quality", "80");
    device_set_option_string(camera->codec_h264[i], "video_bitrate_mode", "0");
    device_set_option_string(camera->codec_h264[i], "video_bitrate", "5000000");
    device_set_option_string(camera->codec_h264[i], "repeat_sequence_header", "1");
    device_set_option_string(camera->codec_h264[i], "h264_i_frame_period", "30");
    device_set_option_string(camera->codec_h264[i], "h264_level", "11");
    device_set_option_string(camera->codec_h264[i], "h264_profile", "4");
    device_set_option_string(camera->codec_h264[i], "h264_minimum_qp_value", "16");
    device_set_option_string(camera->codec_h264[i], "h264_maximum_qp_value", "32");
    device_set_option_list(camera->codec_jpeg[i], camera->options.jpeg.options);
    device_set_option_list(camera->codec_h264[i], camera->options.h264.options);
  }

  // DEVICE_SET_OPTION(camera->camera, EXPOSURE, 2684);
  // DEVICE_SET_OPTION(camera->camera, ANALOGUE_GAIN, 938);
  // DEVICE_SET_OPTION(camera->camera, DIGITAL_GAIN, 512);
  // DEVICE_SET_OPTION(camera->camera, VBLANK, 1636);
  // DEVICE_SET_OPTION(camera->camera, HBLANK, 6906);

  // DEVICE_SET_OPTION(camera->isp_srgb, RED_BALANCE, 2120);
  // DEVICE_SET_OPTION(camera->isp_srgb, BLUE_BALANCE, 1472);
  // DEVICE_SET_OPTION(camera->isp_srgb, DIGITAL_GAIN, 1007);
  return 0;
}

int camera_run(camera_t *camera)
{
  bool running = false;
  return links_loop(camera->links, &running);
}