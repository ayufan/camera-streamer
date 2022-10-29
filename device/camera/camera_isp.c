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

buffer_list_t *camera_configure_isp(camera_t *camera, buffer_list_t *src_capture)
{
  camera->isp = device_v4l2_open("ISP", "/dev/video13");

  buffer_list_t *isp_output = device_open_buffer_list_output(
    camera->isp, src_capture);
  buffer_list_t *isp_capture = device_open_buffer_list_capture(
    camera->isp, "/dev/video14", isp_output, 0, 0, V4L2_PIX_FMT_YUYV, true);

  camera_capture_add_output(camera, src_capture, isp_output);

  return isp_capture;
}
