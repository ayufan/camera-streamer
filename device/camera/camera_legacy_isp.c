#include "camera.h"

#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/links.h"
#include "opts/log.h"
#include "opts/fourcc.h"
#include "device/buffer_list.h"
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

int camera_configure_legacy_isp(camera_t *camera, buffer_list_t *camera_capture, float div)
{
  camera->legacy_isp = device_v4l2_open("ISP", "/dev/video12");
  camera->codec_jpeg = device_v4l2_open("JPEG", "/dev/video31");
  camera->codec_h264 = device_v4l2_open("H264", "/dev/video11");

  buffer_list_t *isp_output = device_open_buffer_list_output(camera->legacy_isp, camera_capture);
  buffer_list_t *isp_capture = device_open_buffer_list_capture(camera->legacy_isp, isp_output, div, V4L2_PIX_FMT_YUYV, true);

  buffer_list_t *jpeg_output = device_open_buffer_list_output(camera->codec_jpeg, isp_capture);
  buffer_list_t *jpeg_capture = device_open_buffer_list_capture(camera->codec_jpeg, jpeg_output, 1.0, V4L2_PIX_FMT_JPEG, true);

  buffer_list_t *h264_output = device_open_buffer_list_output(camera->codec_h264, isp_capture);
  buffer_list_t *h264_capture = device_open_buffer_list_capture(camera->codec_h264, h264_output, 1.0, V4L2_PIX_FMT_H264, true);

  if (!jpeg_capture || !h264_capture) {
    return -1;
  }

  link_t *links = camera->links;
  *links++ = (link_t){ camera_capture, { isp_output } };
  *links++ = (link_t){ isp_capture, { jpeg_output, h264_output }, { write_yuvu, NULL } };
  *links++ = (link_t){ jpeg_capture, { }, { http_jpeg_capture, http_jpeg_needs_buffer } };
  *links++ = (link_t){ h264_capture, { }, { http_h264_capture, http_h264_needs_buffer } };
  return 0;
}
