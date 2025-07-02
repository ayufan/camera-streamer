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

  buffer_format_t fmt = {
    .width = camera->options.width,
    .height = camera->options.height,
    .format = camera->options.format,
    .nbufs = camera->options.nbufs
  };

  buffer_list_t *camera_capture = device_open_buffer_list(camera->camera, true, fmt, true);
  if (!camera_capture) {
    return -1;
  }

  camera_crop_t *crop = NULL;

  if (camera_uses_crop(&camera->options.crop)) {
    camera_capture->do_not_find = true;
    crop = &camera->options.crop;
  }

  return camera_configure_pipeline(camera, camera_capture, crop);
}

static int camera_configure_input_libcamera(camera_t *camera)
{
  camera->camera = device_libcamera_open(camera->name, camera->options.path);
  if (!camera->camera) {
    return -1;
  }

  device_set_rotation(camera->camera, camera->options.vflip, camera->options.hflip);

  camera->camera->opts.allow_dma = camera->options.allow_dma;

  buffer_format_t capture_fmt = {
    .width = camera->options.width,
    .height = camera->options.height,
    .format = camera->options.format,
    .nbufs = camera->options.nbufs,
    .type = BUFFER_TYPE_IMAGE
  };

  bool found = false;

  found = camera_get_scaled_resolution(capture_fmt, &camera->options.snapshot, &capture_fmt, 1);
  if (!found)
    found = camera_get_scaled_resolution(capture_fmt, &camera->options.stream, &capture_fmt, 1);
  if (!found)
    found = camera_get_scaled_resolution(capture_fmt, &camera->options.video, &capture_fmt, 1);

  buffer_list_t *camera_capture = device_open_buffer_list(camera->camera, true, capture_fmt, true);
  if (!camera_capture) {
    return -1;
  }

  buffer_format_t raw_fmt = {
    .width = camera->options.width,
    .height = camera->options.height,
    .nbufs = camera->options.nbufs,
    .type = BUFFER_TYPE_RAW
  };

  buffer_list_t *raw_capture = device_open_buffer_list(camera->camera, true, raw_fmt, true);
  if (!raw_capture) {
    return -1;
  }

  if (buffer_list_alloc_buffers(camera_capture) < 0) {
    return -1;
  }
  if (buffer_list_alloc_buffers(raw_capture) < 0) {
    return -1;
  }

  camera_crop_t *crop = NULL;

  if (camera_uses_crop(&camera->options.crop)) {
    camera_capture->do_not_find = true;
    crop = &camera->options.crop;
  }

  return camera_configure_pipeline(camera, camera_capture, crop);
}

static int camera_configure_input_dummy(camera_t *camera)
{
  camera->camera = device_dummy_open(camera->name, camera->options.path);
  if (!camera->camera) {
    return -1;
  }

  buffer_format_t fmt = {
    .width = camera->options.width,
    .height = camera->options.height,
    .format = camera->options.format,
    .nbufs = camera->options.nbufs
  };

  buffer_list_t *camera_capture = device_open_buffer_list(camera->camera, true, fmt, true);
  if (!camera_capture) {
    return -1;
  }

  camera_crop_t *crop = NULL;

  if (camera_uses_crop(&camera->options.crop)) {
    camera_capture->do_not_find = true;
    crop = &camera->options.crop;
  }

  return camera_configure_pipeline(camera, camera_capture, crop);
}

int camera_configure_input(camera_t *camera)
{
  switch (camera->options.type) {
  case CAMERA_V4L2:
    return camera_configure_input_v4l2(camera);

  case CAMERA_LIBCAMERA:
    return camera_configure_input_libcamera(camera);

  case CAMERA_DUMMY:
    return camera_configure_input_dummy(camera);

  default:
    LOG_INFO(camera, "Unsupported camera type");
    return -1;
  }
}
