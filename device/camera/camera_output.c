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

static const char *jpeg_names[2] = {
  "JPEG",
  "JPEG-LOW"
};

static link_callbacks_t jpeg_callbacks[2] = {
  { .name = "JPEG-CAPTURE", .buf_lock = &http_jpeg },
  { .name = "JPEG-LOW-CAPTURE", .buf_lock = &http_jpeg_lowres }
};

static const char *h264_names[2] = {
  "H264",
  "H264-LOW"
};

static link_callbacks_t h264_callbacks[2] = {
  { .name = "H264-CAPTURE", .buf_lock = &http_h264 },
  { .name = "H264-LOW-CAPTURE", .buf_lock = &http_h264_lowres }
};

static int camera_configure_h264_output(camera_t *camera, buffer_list_t *src_capture, int res)
{
  if (src_capture->fmt.format == V4L2_PIX_FMT_H264) {
    camera_capture_set_callbacks(camera, src_capture, h264_callbacks[res]);
    return 0;
  }

  device_info_t *device = device_list_find_m2m_format(camera->device_list, src_capture->fmt.format, V4L2_PIX_FMT_H264);

  if (!device) {
    LOG_INFO(camera, "Cannot find H264 encoder to convert from '%s'", fourcc_to_string(src_capture->fmt.format).buf);
    return -1;
  }

  camera->codec_h264[res] = device_v4l2_open(h264_names[res], device->path);

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

  device_info_t *device = device_list_find_m2m_format(camera->device_list, src_capture->fmt.format, V4L2_PIX_FMT_JPEG);

  if (!device) {
    device = device_list_find_m2m_format(camera->device_list, src_capture->fmt.format, V4L2_PIX_FMT_MJPEG);
  }

  if (!device) {
    LOG_INFO(camera, "Cannot find JPEG encoder to convert from '%s'", fourcc_to_string(src_capture->fmt.format).buf);
    return -1;
  }

  camera->codec_jpeg[res] = device_v4l2_open(jpeg_names[res], device->path);

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

static void decoder_debug_on_buffer(buffer_t *buf) {
  if (!buf) {
    return;
  }

  static int index = 0;

  char path[256];
  sprintf(path, "/tmp/decoder_capture.%d.%s", index++ % 10, fourcc_to_string(buf->buf_list->fmt.format).buf);

  FILE *fp = fopen(path, "wb");
  if (!fp) {
    return;
  }

  fwrite(buf->start, 1, buf->used, fp);
  fclose(fp);
}

static link_callbacks_t decoder_debug_callbacks = {
  .name = "DECODER-DEBUG-CAPTURE",
  .on_buffer = decoder_debug_on_buffer
};

int camera_configure_decoder(camera_t *camera, buffer_list_t *src_capture)
{
  unsigned decode_formats[] = {
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
  unsigned chosen_format = 0;
  device_info_t *device = device_list_find_m2m_formats(camera->device_list, src_capture->fmt.format, decode_formats, &chosen_format);

  if (!device) {
    LOG_INFO(camera, "Cannot find '%s' decoder", fourcc_to_string(src_capture->fmt.format).buf);
    return -1;
  }

  device_video_force_key(camera->camera);

  camera->decoder = device_v4l2_open("DECODER", device->path);

  buffer_list_t *decoder_output = device_open_buffer_list_output(
    camera->decoder, src_capture);
  buffer_list_t *decoder_capture = device_open_buffer_list_capture(
    camera->decoder, decoder_output, 1.0, chosen_format, true);

  if (getenv("CAMERA_DECODER_DEBUG")) {
    camera_capture_set_callbacks(camera, decoder_capture, decoder_debug_callbacks);
  }

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
