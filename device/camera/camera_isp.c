#include "camera.h"

#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/links.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"
#include "device/buffer_list.h"
#include "http/http.h"

int camera_configure_isp(camera_t *camera, buffer_list_t *src_capture, float high_div, float low_div)
{
  camera->isp = device_v4l2_open("ISP", "/dev/video13");

  buffer_list_t *isp_output = device_open_buffer_list_output(
    camera->isp, src_capture);
  buffer_list_t *isp_capture = device_open_buffer_list_capture2(
    camera->isp, "/dev/video14", isp_output, high_div, V4L2_PIX_FMT_YUYV, true);

  camera_capture_add_output(camera, src_capture, isp_output);

  if (camera_configure_output(camera, isp_capture, 0) < 0) {
    return -1;
  }

  if (low_div > 1) {
    buffer_list_t *isp_lowres_capture = device_open_buffer_list_capture2(
      camera->isp, "/dev/video15", isp_output, low_div, V4L2_PIX_FMT_YUYV, true);

    if (camera_configure_output(camera, isp_lowres_capture, 1) < 0) {
      return -1;
    }
  }

  return 0;
}

static const char *isp_names[2] = {
  "ISP",
  "ISP-LOW"
};

int camera_configure_legacy_isp(camera_t *camera, buffer_list_t *src_capture, float div, int res)
{
  camera->legacy_isp[res] = device_v4l2_open(isp_names[res], "/dev/video12");

  buffer_list_t *isp_output = device_open_buffer_list_output(
    camera->legacy_isp[res], src_capture);
  buffer_list_t *isp_capture = device_open_buffer_list_capture(
    camera->legacy_isp[res], isp_output, div, V4L2_PIX_FMT_YUYV, true);

  camera_capture_add_output(camera, src_capture, isp_output);

  if (camera_configure_output(camera, isp_capture, res) < 0) {
    return -1;
  }

  return 0;
}
