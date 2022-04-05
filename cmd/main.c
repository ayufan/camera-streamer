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

int main(int argc, char *argv[])
{
  camera_t camera;
  int http_fd = -1;
  int ret = -1;
  const char *env;

  if (env = getenv("DEBUG")) {
    log_debug = strstr(env, "1") ? 1 : 0;
  }

  camera_init(&camera);

  //camera.width = 1920; camera.height = 1080;
  strcpy(camera.options.path, "/dev/video2"); camera.options.width = 2328; camera.options.height = 1748; camera.options.format = V4L2_PIX_FMT_SRGGB10P; // 1164x874
  //camera.width = 4656; camera.height = 3496;
  //camera.width = 3840; camera.height = 2160;
  //camera.width = 1280; camera.height = 720;
  strcpy(camera.options.path, "/dev/video0"); camera.options.width = 1920; camera.options.height = 1080; camera.options.format = V4L2_PIX_FMT_YUYV; camera.options.format = V4L2_PIX_FMT_MJPEG; camera.options.allow_dma = false;
  camera.options.nbufs = 1;

  if (camera_open(&camera) < 0) {
    goto error;
  }

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
