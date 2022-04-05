#include "hw/buffer.h"
#include "hw/buffer_list.h"
#include "hw/device.h"
#include "hw/links.h"
#include "hw/v4l2.h"
#include "http/http.h"
#include "camera.h"

#include <signal.h>

http_method_t http_methods[] = {
  { "GET / ", http_index },
  { "GET /snapshot ", http_snapshot },
  { "GET /stream ", http_stream },
  { "GET /?action=snapshot ", http_snapshot },
  { "GET /?action=stream ", http_stream },
  { "GET /video ", http_video_html },
  { "GET /video.h264 ", http_video },
  { "GET /jmuxer.min.js ", http_jmuxer_js },
  { NULL, NULL }
};

bool check_streaming()
{
  return http_jpeg_needs_buffer() || http_h264_needs_buffer();
}

int main(int argc, char *argv[])
{
  camera_t camera;
  int http_fd = -1;
  int ret = -1;

  camera_init(&camera);

  camera.width = 1920; camera.height = 1080;
  //camera.width = 2328; camera.height = 1748; // 1164x874
  //camera.width = 4656; camera.height = 3496;
  //camera.width = 3840; camera.height = 2160;
  camera.nbufs = 4;

  if (camera_open(&camera, "/dev/video0") < 0) {
    goto error;
  }

#if 1
  if (camera_configure_srgb_isp(&camera, 1, 0) < 0) {
    goto error;
  }
#else
  if (camera_configure_srgb_legacy_isp(&camera, 1.3) < 0) {
    goto error;
  }
#endif

  if (camera_set_params(&camera) < 0) {
    goto error;
  }

  http_fd = http_server(9092, 5, http_methods);
  if (http_fd < 0) {
    goto error;
  }

  ret = camera_run(&camera);

error:
  close(http_fd);
  camera_close(&camera);
  return ret;
}
