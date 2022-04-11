#include "camera.h"

#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/links.h"
#include "opts/log.h"
#include "opts/fourcc.h"
#include "device/buffer_list.h"
#include "http/http.h"

static const char *jpeg_names[2] = {
  "JPEG",
  "JPEG-LOW"
};

static link_callbacks_t jpeg_callbacks[2] = {
  { http_jpeg_capture, http_jpeg_needs_buffer },
  { http_jpeg_lowres_capture, http_jpeg_needs_buffer }
};

static const char *h264_names[2] = {
  "H264",
  "H264-LOW"
};

static link_callbacks_t h264_callbacks[2] = {
  { http_h264_capture, http_h264_needs_buffer },
  { http_h264_lowres_capture, http_h264_needs_buffer }
};

static int camera_configure_h264_output(camera_t *camera, buffer_list_t *src_capture, int res)
{
  if (src_capture->fmt.format == V4L2_PIX_FMT_H264) {
    camera_capture_set_callbacks(camera, src_capture, h264_callbacks[res]);
    return 0;
  }

  camera->codec_h264[res] = device_v4l2_open(h264_names[res], "/dev/video11");

  buffer_list_t *output = device_open_buffer_list_output(camera->codec_h264[res], src_capture);
  buffer_list_t *capture = device_open_buffer_list_capture(camera->codec_h264[res], output, 1.0, V4L2_PIX_FMT_H264, true);

  if (!capture) {
    return -1;
  }

  camera_capture_add_output(camera, src_capture, output);
  camera_capture_set_callbacks(camera, capture, h264_callbacks[res]);
  return 0;
}

static int camera_configure_jpeg_output(camera_t *camera, buffer_list_t *src_capture, int res)
{
  if (src_capture->fmt.format == V4L2_PIX_FMT_MJPEG || src_capture->fmt.format == V4L2_PIX_FMT_JPEG) {
    camera_capture_set_callbacks(camera, src_capture, jpeg_callbacks[res]);
    return 0;
  }

  camera->codec_jpeg[res] = device_v4l2_open(jpeg_names[res], "/dev/video31");

  buffer_list_t *output = device_open_buffer_list_output(camera->codec_jpeg[res], src_capture);
  buffer_list_t *capture = device_open_buffer_list_capture(camera->codec_jpeg[res], output, 1.0, V4L2_PIX_FMT_JPEG, true);

  if (!capture) {
    return -1;
  }

  camera_capture_add_output(camera, src_capture, output);
  camera_capture_set_callbacks(camera, capture, jpeg_callbacks[res]);
  return 0;
}

int camera_configure_output(camera_t *camera, buffer_list_t *src_capture, int res)
{
  return camera_configure_h264_output(camera, src_capture, res) == 0 &&
    camera_configure_jpeg_output(camera, src_capture, res) == 0;
}

int camera_configure_output_rescaler(camera_t *camera, buffer_list_t *src_capture, float high_div, float low_div)
{
  if (high_div > 1) {
    if (camera_configure_legacy_isp(camera, src_capture, high_div, 0) < 0) {
      return -1;
    }
  } else if (high_div > 0) {
    if (camera_configure_output(camera, src_capture, 0) < 0) {
      return -1;
    }
  }

  if (low_div > 1) {
    if (camera_configure_legacy_isp(camera, src_capture, low_div, 1) < 0) {
      return -1;
    }
  } else if (low_div > 0) {
    if (camera_configure_output(camera, src_capture, 1) < 0) {
      return -1;
    }
  }

  return 0;
}