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
  camera.width = 2328; camera.height = 1748; // 1164x874
  //camera.width = 4656; camera.height = 3496;
  //camera.width = 3840; camera.height = 2160;
  //camera.width = 1280; camera.height = 720;
  camera.nbufs = 1;
  
#if 1
  camera.format = V4L2_PIX_FMT_SRGGB10P;

  if (camera_open(&camera, "/dev/video2") < 0) {
    goto error;
  }
#else
  //camera.width = 1920; camera.height = 1080;
  camera.width = 1280; camera.height = 720;
  // camera.format = V4L2_PIX_FMT_YUYV;
  camera.format = V4L2_PIX_FMT_MJPEG;
  camera.allow_dma = false;

  if (camera_open(&camera, "/dev/video0") < 0) {
    goto error;
  }
#endif

#if 0
  if (camera_configure_decoder(&camera) < 0) {
    goto error;
  }
#elif 0
  if (camera_configure_direct(&camera) < 0) {
    goto error;
  }
#elif 1
  if (camera_configure_isp(&camera, 3, 0) < 0) {
    goto error;
  }
#else
  if (camera_configure_legacy_isp(&camera, 1.3) < 0) {
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
