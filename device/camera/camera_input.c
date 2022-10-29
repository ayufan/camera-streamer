#include "camera.h"

#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/device_list.h"
#include "device/links.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"

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

  buffer_list_t *camera_capture = device_open_buffer_list(camera->camera, true,
    camera->options.width, camera->options.height, camera->options.format, 0, camera->options.nbufs, true);
  if (!camera_capture) {
    return -1;
  }

  return camera_configure_pipeline(camera, camera_capture);
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
    camera->options.width,
    camera->options.height,
    camera->options.format,
    0,
    camera->options.nbufs,
    true
  );
  if (!camera_capture) {
    return -1;
  }

  return camera_configure_pipeline(camera, camera_capture);
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
