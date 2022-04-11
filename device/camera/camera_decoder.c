#include "camera.h"

#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/links.h"
#include "opts/log.h"
#include "opts/fourcc.h"
#include "device/buffer_list.h"
#include "http/http.h"

int camera_configure_decoder(camera_t *camera, buffer_list_t *src_capture)
{
  device_video_force_key(camera->camera);

  camera->decoder = device_v4l2_open("DECODER", "/dev/video10");

  buffer_list_t *decoder_output = device_open_buffer_list_output(
    camera->decoder, src_capture);
  buffer_list_t *decoder_capture = device_open_buffer_list_capture(
    camera->decoder, decoder_output, 1.0, 0, true);

  camera_capture_add_output(camera, src_capture, decoder_output);

  return camera_configure_output_rescaler(camera, decoder_capture,
    camera->options.high_res_factor, camera->options.low_res_factor);
}
