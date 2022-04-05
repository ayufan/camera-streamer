#include "camera.h"

#include "hw/buffer.h"
#include "hw/buffer_list.h"
#include "hw/device.h"
#include "hw/links.h"
#include "hw/v4l2.h"
#include "hw/buffer_list.h"
#include "http/http.h"

extern bool check_streaming();

int camera_configure_srgb_legacy_isp(camera_t *camera)
{
  if (device_open_buffer_list(camera->camera, true, camera->width, camera->height, V4L2_PIX_FMT_SRGGB10P, camera->nbufs) < 0) {
    return -1;
  }

  buffer_list_t *src = camera->camera->capture_list;

  camera->legacy_isp.isp = device_open("ISP", "/dev/video12");
  camera->codec_jpeg = device_open("JPEG", "/dev/video31");
  camera->codec_h264 = device_open("H264", "/dev/video11");

  if (device_open_buffer_list(camera->legacy_isp.isp, false, src->fmt_width, src->fmt_height, src->fmt_format, camera->nbufs) < 0) {
    return -1;
  }

  src = camera->legacy_isp.isp->capture_list;

  if (device_open_buffer_list(camera->codec_jpeg, false, src->fmt_width, src->fmt_height, src->fmt_format, camera->nbufs) < 0 ||
    device_open_buffer_list(camera->codec_jpeg, true, src->fmt_width, src->fmt_height, V4L2_PIX_FMT_JPEG, camera->nbufs) < 0) {
    return -1;
  }

  if (device_open_buffer_list(camera->codec_h264, false, src->fmt_width, src->fmt_height, src->fmt_format, camera->nbufs) < 0 ||
    device_open_buffer_list(camera->codec_h264, true, src->fmt_width, src->fmt_height, V4L2_PIX_FMT_H264, camera->nbufs) < 0) {
    return -1;
  }

  link_t *links = camera->links;

  *links++ = (link_t){ camera->camera, { camera->legacy_isp.isp }, { NULL, check_streaming } };
  *links++ = (link_t){ camera->legacy_isp.isp, { camera->codec_jpeg, camera->codec_h264 } };
  *links++ = (link_t){ camera->codec_jpeg, { }, { http_jpeg_capture, http_jpeg_needs_buffer } };
  *links++ = (link_t){ camera->codec_h264, { }, { http_h264_capture, http_h264_needs_buffer } };
  return 0;
}
