#include "camera.h"

#include "hw/buffer.h"
#include "hw/buffer_list.h"
#include "hw/device.h"
#include "hw/links.h"
#include "hw/v4l2.h"
#include "hw/buffer_list.h"
#include "http/http.h"

void write_yuvu(buffer_t *buffer);

int camera_configure_isp(camera_t *camera, float high_div, float low_div)
{
  buffer_list_t *src = camera->camera->capture_list;

  camera->isp_srgb = device_open("ISP", "/dev/video13");
  camera->isp_yuuv = device_open("ISP-YUUV", "/dev/video14");
  camera->isp_yuuv->output_device = camera->isp_srgb;
  camera->codec_jpeg = device_open("JPEG", "/dev/video31");
  camera->codec_h264 = device_open("H264", "/dev/video11");

  if (device_open_buffer_list_output(camera->isp_srgb, src) < 0 ||
    device_open_buffer_list_capture(camera->isp_yuuv, src, high_div, V4L2_PIX_FMT_YUYV, true) < 0) {
    return -1;
  }

  if (low_div >= 1) {
    camera->isp_yuuv_low = device_open("ISP-YUUV-LOW", "/dev/video15");
    camera->isp_yuuv_low->output_device = camera->isp_srgb;

    if (device_open_buffer_list_capture(camera->isp_yuuv_low, src, low_div, V4L2_PIX_FMT_YUYV, true) < 0) {
      return -1;
    }

    src = camera->isp_yuuv_low->capture_list;
  } else {
    src = camera->isp_yuuv->capture_list;
  }

  if (device_open_buffer_list_output(camera->codec_jpeg, src) < 0 ||
    device_open_buffer_list_capture(camera->codec_jpeg, src, 1.0, V4L2_PIX_FMT_JPEG, true) < 0) {
    return -1;
  }

  if (device_open_buffer_list_output(camera->codec_h264, src) < 0 ||
    device_open_buffer_list_capture(camera->codec_h264, src, 1.0, V4L2_PIX_FMT_H264, true) < 0) {
    return -1;
  }

  // DEVICE_SET_OPTION(camera->isp_srgb, RED_BALANCE, 2120);
  // DEVICE_SET_OPTION(camera->isp_srgb, BLUE_BALANCE, 1472);
  // DEVICE_SET_OPTION(camera->isp_srgb, DIGITAL_GAIN, 1007);

  link_t *links = camera->links;

  *links++ = (link_t){ camera->camera->capture_list, { camera->isp_srgb->output_list } };

  if (camera->isp_yuuv_low) {
    *links++ = (link_t){ camera->isp_yuuv->capture_list, { } };
    *links++ = (link_t){ camera->isp_yuuv_low->capture_list, { camera->codec_jpeg->output_list, camera->codec_h264->output_list }, { write_yuvu } };
  } else {
    *links++ = (link_t){ camera->isp_yuuv->capture_list, { camera->codec_jpeg->output_list, camera->codec_h264->output_list }, { write_yuvu } };
  }

  *links++ = (link_t){ camera->codec_jpeg->capture_list, { }, { http_jpeg_capture, http_jpeg_needs_buffer } };
  *links++ = (link_t){ camera->codec_h264->capture_list, { }, { http_h264_capture, http_h264_needs_buffer } };
  return 0;
}
