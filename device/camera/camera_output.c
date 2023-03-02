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

#define MATCH_ALIGN_SIZE 32

static bool camera_output_matches_capture(buffer_list_t *capture, unsigned target_height, unsigned format)
{
  if (target_height && abs((int)capture->fmt.height - (int)target_height) > MATCH_ALIGN_SIZE) {
    return false;
  }

  if (format != capture->fmt.format) {
    return false;
  }

  return true;
}

static buffer_list_t *camera_find_capture(camera_t *camera, unsigned target_height, unsigned format)
{
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!camera->devices[i])
      continue;

    device_t *device = camera->devices[i];
    for (int j = 0; j < device->n_capture_list; j++) {
      buffer_list_t *capture_list = device->capture_lists[j];

      if (camera_output_matches_capture(capture_list, target_height, format)) {
        return capture_list;
      }
    }
  }

  return NULL;
}

static buffer_list_t *camera_find_capture2(camera_t *camera, unsigned target_height, unsigned formats[])
{
  for (int i = 0; formats[i]; i++) {
    buffer_list_t *capture = camera_find_capture(camera, target_height, formats[i]);
    if (capture) {
      return capture;
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

#define OUTPUT_RESCALLER_SIZE 32

int camera_configure_output(camera_t *camera, buffer_list_t *camera_capture, const char *name, camera_output_options_t *options, unsigned formats[], link_callbacks_t callbacks, device_t **device)
{
  buffer_format_t selected_format = {0};
  buffer_format_t rescalled_format = {0};

  if (!camera_get_scaled_resolution(camera_capture->fmt, options, &selected_format, 1)) {
    return 0;
  }

  if (!camera_get_scaled_resolution(camera_capture->fmt, options, &rescalled_format, RESCALLER_BLOCK_SIZE)) {
    return 0;
  }

  // find an existing format
  buffer_list_t *src_capture = camera_find_capture2(camera, selected_format.height, formats);
  if (src_capture) {
    camera_capture_add_callbacks(camera, src_capture, callbacks);
    return 0;
  }

  // Try to find exact capture
  src_capture = camera_find_capture2(camera, rescalled_format.height, rescalled_formats);

  // Try to re-scale capture
  if (!src_capture) {
    buffer_list_t *other_capture = camera_find_capture2(camera, 0, rescalled_formats);

    if (other_capture) {
      src_capture = camera_configure_rescaller(camera, other_capture, name, rescalled_format.height, rescalled_formats);
    }
  }

  // Try to decode output
  if (!src_capture) {
    buffer_list_t *decoded_capture = NULL;

    switch (camera_capture->fmt.format) {
    case V4L2_PIX_FMT_SRGGB10P:
    case V4L2_PIX_FMT_SGRBG10P:
    case V4L2_PIX_FMT_SBGGR10P:
    case V4L2_PIX_FMT_SRGGB10:
    case V4L2_PIX_FMT_SGRBG10:
      decoded_capture = camera_configure_isp(camera, camera_capture);
      break;

    case V4L2_PIX_FMT_MJPEG:
    case V4L2_PIX_FMT_H264:
      decoded_capture = camera_configure_decoder(camera, camera_capture);
      break;
    }

    // Now, do we have exact match
    src_capture = camera_find_capture2(camera, selected_format.height, rescalled_formats);

    if (!src_capture) {
      src_capture = camera_find_capture2(camera, rescalled_format.height, rescalled_formats);
    }

    // Otherwise rescalle decoded output
    if (!src_capture && decoded_capture) {
      src_capture = camera_configure_rescaller(camera, decoded_capture, name, selected_format.height, rescalled_formats);
    }
  }

  if (!src_capture) {
    LOG_INFO(camera, "Cannot find source for '%s' for one of the formats '%s'.", name, many_fourcc_to_string(formats).buf);
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
  buffer_list_t *capture = device_open_buffer_list_capture2(*device, NULL, output, chosen_format, true);

  if (!capture) {
    return -1;
  }

  camera_capture_add_output(camera, src_capture, output);
  camera_capture_add_callbacks(camera, capture, callbacks);
  return 0;
}
