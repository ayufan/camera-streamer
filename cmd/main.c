#include "hw/buffer.h"
#include "hw/buffer_list.h"
#include "hw/device.h"
#include "hw/links.h"
#include "hw/v4l2.h"
#include "http/http.h"
#include "opts/opts.h"
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

camera_options_t camera_options = {
  .path = "/dev/video0",
  .width = 1920,
  .height = 1080,
  .format = V4L2_PIX_FMT_SRGGB10P,
  .nbufs = 4,
  .fps = 30,
  .allow_dma = true
};

http_server_options_t http_options = {
  .port = 9092,
  .maxcons = 10
};

option_t all_options[] = {
  DEFINE_OPTION_PTR(camera, path, "%s"),
  DEFINE_OPTION(camera, width, uint),
  DEFINE_OPTION(camera, height, uint),
  DEFINE_OPTION(camera, format, uint),
  DEFINE_OPTION(camera, nbufs, uint),
  DEFINE_OPTION(camera, fps, uint),
  DEFINE_OPTION(camera, allow_dma, bool),
  DEFINE_OPTION(http, port, uint),
  DEFINE_OPTION(http, maxcons, uint),
  {}
};

int main(int argc, char *argv[])
{
  camera_t camera;
  int http_fd = -1;
  int ret = -1;
  const char *env;

  if (parse_opts(all_options, argc, argv) < 0) {
    return -1;
  }

  if (env = getenv("DEBUG")) {
    log_debug = strstr(env, "1") ? 1 : 0;
  }

  camera_init(&camera);
  camera.options = camera_options;

  // //camera.width = 1920; camera.height = 1080;
  // strcpy(camera.options.path, "/dev/video2"); camera.options.width = 2328; camera.options.height = 1748; camera.options.format = V4L2_PIX_FMT_SRGGB10P; // 1164x874
  // //camera.width = 4656; camera.height = 3496;
  // //camera.width = 3840; camera.height = 2160;
  // //camera.width = 1280; camera.height = 720;
  // strcpy(camera.options.path, "/dev/video0"); camera.options.width = 1920; camera.options.height = 1080; camera.options.format = V4L2_PIX_FMT_YUYV; camera.options.format = V4L2_PIX_FMT_MJPEG; camera.options.allow_dma = false;
  // camera.options.nbufs = 1;

  if (camera_open(&camera) < 0) {
    goto error;
  }

  if (camera_set_params(&camera) < 0) {
    goto error;
  }

  http_fd = http_server(&http_options, http_methods);
  if (http_fd < 0) {
    goto error;
  }

  ret = camera_run(&camera);

error:
  close(http_fd);
  camera_close(&camera);
  return ret;
}
