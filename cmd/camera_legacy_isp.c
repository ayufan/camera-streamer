#include "camera.h"

#include "hw/buffer.h"
#include "hw/buffer_list.h"
#include "hw/device.h"
#include "hw/links.h"
#include "hw/v4l2.h"
#include "hw/buffer_list.h"
#include "http/http.h"

void write_yuvu(buffer_t *buffer)
{
#if 0
  FILE *fp = fopen("/tmp/capture-yuyv.raw.tmp", "wb");
  if (fp) {
    fwrite(buffer->start, 1, buffer->used, fp);
    fclose(fp);
  }
  rename("/tmp/capture-yuyv.raw.tmp", "/tmp/capture-yuyv.raw");
#endif
}

int camera_configure_legacy_isp(camera_t *camera, float div)
{
  buffer_list_t *src = camera->camera->capture_list;

  camera->legacy_isp = device_open("ISP", "/dev/video12");
  camera->codec_jpeg = device_open("JPEG", "/dev/video31");
  camera->codec_h264 = device_open("H264", "/dev/video11");

  if (device_open_buffer_list_output(camera->legacy_isp, src) < 0 ||
    device_open_buffer_list_capture(camera->legacy_isp, src, div, V4L2_PIX_FMT_YUYV, true) < 0) {
    return -1;
  }

  src = camera->legacy_isp->capture_list;

  if (device_open_buffer_list_output(camera->codec_jpeg, src) < 0 ||
    device_open_buffer_list_capture(camera->codec_jpeg, src, 1.0, V4L2_PIX_FMT_JPEG, true) < 0) {
    return -1;
  }

  if (device_open_buffer_list_output(camera->codec_h264, src) < 0 ||
    device_open_buffer_list_capture(camera->codec_h264, src, 1.0, V4L2_PIX_FMT_H264, true) < 0) {
    return -1;
  }

  link_t *links = camera->links;

  *links++ = (link_t){ camera->camera, { camera->legacy_isp } };
  *links++ = (link_t){ camera->legacy_isp, { camera->codec_jpeg, camera->codec_h264 }, { write_yuvu, NULL } };
  *links++ = (link_t){ camera->codec_jpeg, { }, { http_jpeg_capture, http_jpeg_needs_buffer } };
  *links++ = (link_t){ camera->codec_h264, { }, { http_h264_capture, http_h264_needs_buffer } };
  return 0;
}
