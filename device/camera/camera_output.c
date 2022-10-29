#include "camera.h"

#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/device_list.h"
#include "device/links.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"
#include "device/buffer_list.h"
#include "util/http/http.h"
#include "output/rtsp/rtsp.h"
#include "output/output.h"

static bool camera_output_matches_capture(buffer_list_t *capture, unsigned target_height, unsigned formats[])
{
  if (target_height && capture->fmt.height != target_height && capture->fmt.height != camera_rescaller_align_size(target_height)) {
    return false;
  }

  for (int i = 0; formats[i]; i++) {
    if (formats[i] == capture->fmt.format)
      return true;
  }

  return false;
}

static buffer_list_t *camera_find_capture(camera_t *camera, unsigned target_height, unsigned formats[])
{
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!camera->devices[i])
      continue;

    device_t *device = camera->devices[i];
    for (int j = 0; j < device->n_capture_list; j++) {
      buffer_list_t *capture_list = device->capture_lists[j];

      if (camera_output_matches_capture(capture_list, target_height, formats)) {
        return capture_list;
      }
    }
  }

  return NULL;
}

static unsigned rescalled_formats[] =
{
  // best quality
  V4L2_PIX_FMT_YUYV,

  // medium quality
  V4L2_PIX_FMT_YUV420,
  V4L2_PIX_FMT_NV12,

  // low quality
  V4L2_PIX_FMT_NV21,
  V4L2_PIX_FMT_YVU420,

  0
};

int camera_configure_output(camera_t *camera, const char *name, unsigned target_height, unsigned formats[], link_callbacks_t callbacks, device_t **device)
{
  buffer_list_t *src_capture = camera_find_capture(camera, target_height, formats);
  if (src_capture) {
    camera_capture_add_callbacks(camera, src_capture, callbacks);
    return 0;
  }

  src_capture = camera_find_capture(camera, target_height, rescalled_formats);
  if (!src_capture) {
    // Try to find re-scallabe output
    src_capture = camera_find_capture(camera, 0, rescalled_formats);
    if (src_capture) {
      src_capture = camera_configure_rescaller(camera, src_capture, name, target_height, rescalled_formats);
    }
  }

  if (!src_capture) {
    return -1;
  }

  unsigned chosen_format = 0;
  device_info_t *device_info = device_list_find_m2m_formats(camera->device_list, src_capture->fmt.format, formats, &chosen_format);

  if (!device_info) {
    LOG_INFO(camera, "Cannot find encoder to convert from '%s'", fourcc_to_string(src_capture->fmt.format).buf);
    return -1;
  }

  *device = device_v4l2_open(name, device_info->path);

  buffer_list_t *output = device_open_buffer_list_output(*device, src_capture);
  buffer_list_t *capture = device_open_buffer_list_capture(*device, NULL, output, 0, 0, chosen_format, true);

  if (!capture) {
    return -1;
  }

  camera_capture_add_output(camera, src_capture, output);
  camera_capture_add_callbacks(camera, capture, callbacks);
  return 0;
}
