#include "camera.h"

#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/links.h"
#include "opts/log.h"
#include "opts/fourcc.h"

int camera_configure_libcamera(camera_t *camera)
{
  camera->camera = device_libcamera_open(camera->name, camera->options.path);
  if (!camera->camera) {
    goto error;
  }

  camera->camera->opts.allow_dma = camera->options.allow_dma;

  buffer_list_t *camera_capture = device_open_buffer_list(camera->camera, true, camera->options.width / camera->options.high_res_factor, camera->options.height / camera->options.high_res_factor, camera->options.format, 0, camera->options.nbufs, true);
  if (!camera_capture) {
    goto error;
  }
  camera_capture->do_timestamps = true;

  if (camera->options.fps > 0) {
    camera_capture->fmt.interval_us = 1000 * 1000 / camera->options.fps;
  }

  if (camera_configure_direct(camera, camera_capture) < 0) {
    goto error;
  }

  return 0;

error:
  return -1;
}