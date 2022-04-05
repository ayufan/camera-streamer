#include "camera.h"

#include "hw/buffer.h"
#include "hw/buffer_list.h"
#include "hw/device.h"
#include "hw/links.h"
#include "hw/v4l2.h"
#include "hw/buffer_list.h"
#include "http/http.h"

extern bool check_streaming();

int camera_configure_srgb_isp(camera_t *camera, bool use_half)
{
  if (device_open_buffer_list(camera->camera, true, camera->width, camera->height, V4L2_PIX_FMT_SRGGB10P, camera->nbufs) < 0) {
    return -1;
  }

  buffer_list_t *src = camera->camera->capture_list;

  camera->isp.isp_srgb = device_open("ISP", "/dev/video13");
  camera->isp.isp_yuuv = device_open("ISP-YUUV", "/dev/video14");
  camera->codec_jpeg = device_open("JPEG", "/dev/video31");
  camera->codec_h264 = device_open("H264", "/dev/video11");

  if (device_open_buffer_list(camera->isp.isp_srgb, false, src->fmt_width, src->fmt_height, src->fmt_format, camera->nbufs) < 0 ||
    device_open_buffer_list(camera->isp.isp_yuuv, true, src->fmt_width, src->fmt_height, V4L2_PIX_FMT_YUYV, camera->nbufs) < 0) {
    return -1;
  }

  if (use_half) {
    camera->isp.isp_yuuv_low = device_open("ISP-YUUV-LOW", "/dev/video15");

    if (device_open_buffer_list(camera->isp.isp_yuuv_low, true, src->fmt_width / 2, src->fmt_height / 2, V4L2_PIX_FMT_YUYV, camera->nbufs) < 0) {
      return -1;
    }

    src = camera->isp.isp_yuuv_low->capture_list;
  } else {
    src = camera->isp.isp_yuuv->capture_list;
  }

  if (device_open_buffer_list(camera->codec_jpeg, false, src->fmt_width, src->fmt_height, src->fmt_format, camera->nbufs) < 0 ||
    device_open_buffer_list(camera->codec_jpeg, true, src->fmt_width, src->fmt_height, V4L2_PIX_FMT_JPEG, camera->nbufs) < 0) {
    return -1;
  }

  if (device_open_buffer_list(camera->codec_h264, false, src->fmt_width, src->fmt_height, src->fmt_format, camera->nbufs) < 0 ||
    device_open_buffer_list(camera->codec_h264, true, src->fmt_width, src->fmt_height, V4L2_PIX_FMT_H264, camera->nbufs) < 0) {
    return -1;
  }

  DEVICE_SET_OPTION(camera->isp.isp_srgb, RED_BALANCE, 2120);
  DEVICE_SET_OPTION(camera->isp.isp_srgb, BLUE_BALANCE, 1472);
  DEVICE_SET_OPTION(camera->isp.isp_srgb, DIGITAL_GAIN, 1007);

  link_t *links = camera->links;

  *links++ = (link_t){ camera->camera, { camera->isp.isp_srgb }, { NULL, check_streaming } };

  if (use_half) {
    *links++ = (link_t){ camera->isp.isp_yuuv, { } };
    *links++ = (link_t){ camera->isp.isp_yuuv_low, { camera->codec_jpeg, camera->codec_h264 } };
  } else {
    *links++ = (link_t){ camera->isp.isp_yuuv, { camera->codec_jpeg, camera->codec_h264 } };
  }

  *links++ = (link_t){ camera->codec_jpeg, { }, { http_jpeg_capture, http_jpeg_needs_buffer } };
  *links++ = (link_t){ camera->codec_h264, { }, { http_h264_capture, http_h264_needs_buffer } };
  return 0;
}
