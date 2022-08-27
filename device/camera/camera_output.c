#include "camera.h"

#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/links.h"
#include "opts/log.h"
#include "opts/fourcc.h"
#include "device/buffer_list.h"
#include "http/http.h"
#include "rtsp/rtsp.h"

static const char *jpeg_names[2] = {
  "JPEG",
  "JPEG-LOW"
};

static link_callbacks_t jpeg_callbacks[2] = {
  { "JPEG-CAPTURE", http_jpeg_capture, http_jpeg_needs_buffer },
  { "JPEG-LOW-CAPTURE", http_jpeg_lowres_capture, http_jpeg_needs_buffer }
};

static const char *h264_names[2] = {
  "H264",
  "H264-LOW"
};

static void h264_capture(buffer_t *buf)
{
  http_h264_capture(buf);
  rtsp_h264_capture(buf);
}

static void h264_lowres_capture(buffer_t *buf)
{
  http_h264_lowres_capture(buf);
  rtsp_h264_low_res_capture(buf);
}

static bool h264_needs_buffer()
{
  return http_h264_needs_buffer() | rtsp_h264_needs_buffer();
}

static link_callbacks_t h264_callbacks[2] = {
  { "H264-CAPTURE", h264_capture, h264_needs_buffer },
  { "H264-LOW-CAPTURE", h264_lowres_capture, h264_needs_buffer }
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
  if (camera_configure_h264_output(camera, src_capture, res) < 0 ||
    camera_configure_jpeg_output(camera, src_capture, res) < 0) {
      return -1;
    }

  return 0;
}

int camera_configure_output_rescaler2(camera_t *camera, buffer_list_t *src_capture, float div, int res)
{
  if (div > 1) {
    return camera_configure_legacy_isp(camera, src_capture, div, res);
  } else if (div > 0) {
    return camera_configure_output(camera, src_capture, 0);
  } else {
    return 0;
  }
}

int camera_configure_output_rescaler(camera_t *camera, buffer_list_t *src_capture, float high_div, float low_div)
{
  if (camera_configure_output_rescaler2(camera, src_capture, high_div, 0) < 0 ||
    camera_configure_output_rescaler2(camera, src_capture, low_div, 1) < 0) {
    return -1;
  }

  return 0;
}

int camera_configure_decoder(camera_t *camera, buffer_list_t *src_capture)
{
  device_video_force_key(camera->camera);

  camera->decoder = device_v4l2_open("DECODER", "/dev/video10");

  buffer_list_t *decoder_output = device_open_buffer_list_output(
    camera->decoder, src_capture);
  buffer_list_t *decoder_capture = device_open_buffer_list_capture(
    camera->decoder, decoder_output, 1.0, 0, true);

  camera_capture_add_output(camera, src_capture, decoder_output);

  if (camera->options.high_res_factor <= 1 && (src_capture->fmt.format == V4L2_PIX_FMT_JPEG || src_capture->fmt.format == V4L2_PIX_FMT_MJPEG)) {
    camera_capture_set_callbacks(camera, src_capture, jpeg_callbacks[0]);

    if (camera_configure_h264_output(camera, decoder_capture, 0) < 0)
      return -1;
  } else if (camera->options.high_res_factor <= 1 && src_capture->fmt.format == V4L2_PIX_FMT_H264) {
    camera_capture_set_callbacks(camera, src_capture, h264_callbacks[0]);

    if (camera_configure_jpeg_output(camera, decoder_capture, 0) < 0)
      return -1;
  } else if (camera_configure_output_rescaler2(camera, decoder_capture, camera->options.high_res_factor, 0) < 0) {
    return -1;
  }

  if (camera->options.low_res_factor > 1 && camera_configure_output_rescaler2(camera, decoder_capture, camera->options.low_res_factor, 1) < 0) {
    return -1;
  }

  return 0;
}
