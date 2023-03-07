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
#include "output/rtsp/rtsp.h"
#include "output/output.h"

static unsigned decoder_formats[] =
{
  // best quality
  V4L2_PIX_FMT_YUYV,

  // medium quality
  V4L2_PIX_FMT_NV12,
  V4L2_PIX_FMT_YUV420,

  // low quality
  V4L2_PIX_FMT_NV21,
  V4L2_PIX_FMT_YVU420,
  0
};

buffer_list_t *camera_configure_decoder(camera_t *camera, buffer_list_t *src_capture)
{
  unsigned chosen_format = 0;
  device_info_t *device = device_list_find_m2m_formats(camera->device_list, src_capture->fmt.format, decoder_formats, &chosen_format);

  if (!device) {
    LOG_INFO(camera, "Cannot find '%s' decoder", fourcc_to_string(src_capture->fmt.format).buf);
    return NULL;
  }

  device_video_force_key(camera->camera);

  camera->decoder = device_v4l2_open("DECODER", device->path);

  buffer_list_t *decoder_output = device_open_buffer_list_output(
    camera->decoder, src_capture);
  buffer_list_t *decoder_capture = device_open_buffer_list_capture2(
    camera->decoder, NULL, decoder_output, chosen_format, true);

  camera_debug_capture(camera, decoder_capture);
  camera_capture_add_output(camera, src_capture, decoder_output);

  return decoder_capture;
}
