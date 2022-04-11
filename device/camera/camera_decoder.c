#include "camera.h"

#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/links.h"
#include "opts/log.h"
#include "opts/fourcc.h"
#include "device/buffer_list.h"
#include "http/http.h"

int camera_configure_decoder(camera_t *camera, buffer_list_t *camera_capture)
{
  buffer_list_t *src = camera_capture;
  device_video_force_key(camera->camera);

  camera->decoder = device_v4l2_open("DECODER", "/dev/video10");

  buffer_list_t *decoder_output = device_open_buffer_list_output(camera->decoder, src);
  buffer_list_t *decoder_capture = device_open_buffer_list_capture(camera->decoder, decoder_output, 1.0, 0, true);

  link_t *links = camera->links;
  link_t *camera_link = &*links++;
  link_t *decoder_link = &*links++;

  *camera_link = (link_t){ camera_capture, { decoder_output }, {} };
  *decoder_link = (link_t){ decoder_capture, { }, { } };
  buffer_list_t **decoder_sinks = &decoder_link->sinks[0];

  if (camera_capture->fmt.format == V4L2_PIX_FMT_MJPEG || camera_capture->fmt.format == V4L2_PIX_FMT_JPEG) {
    camera_link->callbacks.on_buffer = http_jpeg_capture;
    camera_link->callbacks.validate_buffer = http_jpeg_needs_buffer;
  } else {
    camera->codec_jpeg = device_v4l2_open("JPEG", "/dev/video31");

    buffer_list_t *jpeg_output = device_open_buffer_list_output(camera->codec_jpeg, decoder_capture);
    buffer_list_t *jpeg_capture = device_open_buffer_list_capture(camera->codec_jpeg, jpeg_output, 1.0, V4L2_PIX_FMT_JPEG, true);

    if (!jpeg_capture) {
      return -1;
    }

    *decoder_sinks++ = jpeg_output;
    *links++ = (link_t){ jpeg_capture, { }, { http_jpeg_capture, http_jpeg_needs_buffer } };
  }

  if (camera_capture->fmt.format == V4L2_PIX_FMT_H264) {
    camera_link->callbacks.on_buffer = http_h264_capture;
    camera_link->callbacks.validate_buffer = http_h264_needs_buffer;
  } else {
    camera->codec_h264 = device_v4l2_open("H264", "/dev/video11");

    buffer_list_t *h264_output = device_open_buffer_list_output(camera->codec_jpeg, decoder_capture);
    buffer_list_t *h264_capture = device_open_buffer_list_capture(camera->codec_jpeg, h264_output, 1.0, V4L2_PIX_FMT_H264, true);

    if (!h264_output) {
      return -1;
    }

    *decoder_sinks++ = h264_output;
    *links++ = (link_t){ h264_capture, { }, { http_h264_capture, http_h264_needs_buffer } };
  }

  if (device_set_decoder_start(camera->decoder, true) < 0) {
    return -1;
  }
  return 0;
}
