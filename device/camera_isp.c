#include "camera.h"

#include "device/hw/buffer.h"
#include "device/hw/buffer_list.h"
#include "device/hw/device.h"
#include "device/hw/links.h"
#include "device/hw/v4l2.h"
#include "device/hw/buffer_list.h"
#include "http/http.h"

void write_yuvu(buffer_t *buffer);

int camera_configure_isp(camera_t *camera, float high_div, float low_div)
{
  camera->isp_srgb = device_open("ISP", "/dev/video13");
  camera->isp_yuuv = device_open("ISP-YUUV", "/dev/video14");
  camera->codec_jpeg = device_open("JPEG", "/dev/video31");
  camera->codec_h264 = device_open("H264", "/dev/video11");

  if (device_open_buffer_list_output(camera->isp_srgb, camera->camera->capture_list) < 0 ||
    device_open_buffer_list_capture(camera->isp_yuuv, camera->camera->capture_list, high_div, V4L2_PIX_FMT_YUYV, true) < 0) {
    return -1;
  }

  camera->isp_yuuv->output_device = camera->isp_srgb;

  link_t *links = camera->links;
  *links++ = (link_t){ camera->camera->capture_list, { camera->isp_srgb->output_list } };

  buffer_list_t *src = camera->isp_yuuv->capture_list;

  if (device_open_buffer_list_output(camera->codec_jpeg, src) < 0 ||
    device_open_buffer_list_capture(camera->codec_jpeg, src, 1.0, V4L2_PIX_FMT_JPEG, true) < 0) {
    return -1;
  }

  if (device_open_buffer_list_output(camera->codec_h264, src) < 0 ||
    device_open_buffer_list_capture(camera->codec_h264, src, 1.0, V4L2_PIX_FMT_H264, true) < 0) {
    return -1;
  }

  *links++ = (link_t){ camera->isp_yuuv->capture_list, { camera->codec_jpeg->output_list, camera->codec_h264->output_list }, { write_yuvu } };
  *links++ = (link_t){ camera->codec_jpeg->capture_list, { }, { http_jpeg_capture, http_jpeg_needs_buffer } };
  *links++ = (link_t){ camera->codec_h264->capture_list, { }, { http_h264_capture, http_h264_needs_buffer } };

  // all done
  if (low_div < 1) {
    return 0;
  }

  camera->isp_yuuv_lowres = device_open("ISP-YUUV-LOW", "/dev/video15");
  camera->codec_jpeg_lowres = device_open("JPEG-LOW", "/dev/video31");
  camera->codec_h264_lowres = device_open("H264-LOW", "/dev/video11");

  if (device_open_buffer_list_capture(camera->isp_yuuv_lowres, camera->camera->capture_list, low_div, V4L2_PIX_FMT_YUYV, true) < 0) {
    return -1;
  }

  camera->isp_yuuv_lowres->output_device = camera->isp_srgb;
  src = camera->isp_yuuv_lowres->capture_list;

  if (device_open_buffer_list_output(camera->codec_jpeg_lowres, src) < 0 ||
    device_open_buffer_list_capture(camera->codec_jpeg_lowres, src, 1.0, V4L2_PIX_FMT_JPEG, true) < 0) {
    return -1;
  }

  if (device_open_buffer_list_output(camera->codec_h264_lowres, src) < 0 ||
    device_open_buffer_list_capture(camera->codec_h264_lowres, src, 1.0, V4L2_PIX_FMT_H264, true) < 0) {
    return -1;
  }

  *links++ = (link_t){ camera->isp_yuuv_lowres->capture_list, { camera->codec_jpeg_lowres->output_list, camera->codec_h264_lowres->output_list }, { write_yuvu } };
  *links++ = (link_t){ camera->codec_jpeg_lowres->capture_list, { }, { http_jpeg_lowres_capture, http_jpeg_needs_buffer } };
  *links++ = (link_t){ camera->codec_h264_lowres->capture_list, { }, { http_h264_lowres_capture, http_h264_needs_buffer } };
  return 0;
}
