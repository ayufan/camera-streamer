#include "camera.h"

#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/links.h"
#include "opts/log.h"
#include "opts/fourcc.h"
#include "device/buffer_list.h"
#include "http/http.h"

void write_yuvu(buffer_t *buffer);

int camera_configure_isp(camera_t *camera, buffer_list_t *camera_capture, float high_div, float low_div)
{
  link_t *links = camera->links;

  camera->isp_srgb = device_v4l2_open("ISP", "/dev/video13");
  camera->isp_yuuv = device_v4l2_open("ISP-YUUV", "/dev/video14");
  if (camera->isp_yuuv) {
    camera->isp_yuuv->output_device = camera->isp_srgb;
  }
  camera->codec_jpeg = device_v4l2_open("JPEG", "/dev/video31");
  camera->codec_h264 = device_v4l2_open("H264", "/dev/video11");

  buffer_list_t *isp_output = device_open_buffer_list_output(camera->isp_srgb, camera_capture);
  buffer_list_t *isp_capture = device_open_buffer_list_capture(camera->isp_yuuv, isp_output, high_div, V4L2_PIX_FMT_YUYV, true);

  *links++ = (link_t){ camera_capture, { isp_output } };

  buffer_list_t *jpeg_output = device_open_buffer_list_output(camera->codec_jpeg, isp_capture);
  buffer_list_t *jpeg_capture = device_open_buffer_list_capture(camera->codec_jpeg, jpeg_output, 1.0, V4L2_PIX_FMT_JPEG, true);

  buffer_list_t *h264_output = device_open_buffer_list_output(camera->codec_h264, isp_capture);
  buffer_list_t *h264_capture = device_open_buffer_list_capture(camera->codec_h264, h264_output, 1.0, V4L2_PIX_FMT_H264, true);

  if (!jpeg_capture || !h264_capture) {
    return -1;
  }

  *links++ = (link_t){ isp_capture, { jpeg_output, h264_output }, { write_yuvu } };
  *links++ = (link_t){ jpeg_capture, { }, { http_jpeg_capture, http_jpeg_needs_buffer } };
  *links++ = (link_t){ h264_capture, { }, { http_h264_capture, http_h264_needs_buffer } };

  // all done
  if (low_div < 1) {
    return 0;
  }

  camera->isp_yuuv_lowres = device_v4l2_open("ISP-YUUV-LOW", "/dev/video15");
  if (camera->isp_yuuv_lowres) {
    camera->isp_yuuv_lowres->output_device = camera->isp_srgb;
  }
  camera->codec_jpeg_lowres = device_v4l2_open("JPEG-LOW", "/dev/video31");
  camera->codec_h264_lowres = device_v4l2_open("H264-LOW", "/dev/video11");

  buffer_list_t *isp_lowres_capture = device_open_buffer_list_capture(camera->isp_yuuv_lowres, isp_output, low_div, V4L2_PIX_FMT_YUYV, true);

  buffer_list_t *jpeg_lowres_output = device_open_buffer_list_output(camera->codec_jpeg_lowres, isp_lowres_capture);
  buffer_list_t *jpeg_lowres_capture = device_open_buffer_list_capture(camera->codec_jpeg_lowres, jpeg_lowres_output, 1.0, V4L2_PIX_FMT_JPEG, true);

  buffer_list_t *h264_lowres_output = device_open_buffer_list_output(camera->codec_h264_lowres, isp_lowres_capture);
  buffer_list_t *h264_lowres_capture = device_open_buffer_list_capture(camera->codec_h264_lowres, h264_lowres_output, 1.0, V4L2_PIX_FMT_H264, true);

  if (!jpeg_lowres_capture || !h264_lowres_capture) {
    return -1;
  }

  *links++ = (link_t){ isp_capture, { jpeg_lowres_output, h264_lowres_output }, { write_yuvu } };
  *links++ = (link_t){ jpeg_lowres_capture, { }, { http_jpeg_lowres_capture, http_jpeg_needs_buffer } };
  *links++ = (link_t){ h264_lowres_capture, { }, { http_h264_lowres_capture, http_h264_needs_buffer } };
  return 0;
}
