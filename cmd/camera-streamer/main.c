#include "util/http/http.h"
#include "util/opts/opts.h"
#include "util/opts/log.h"
#include "device/camera/camera.h"
#include "output/rtsp/rtsp.h"
#include "output/webrtc/webrtc.h"

#include <signal.h>
#include <unistd.h>

extern option_t all_options[];
extern camera_options_t camera_options;
extern http_server_options_t http_options;
extern http_method_t http_methods[];
extern rtsp_options_t rtsp_options;

camera_t *camera;

int main(int argc, char *argv[])
{
  int http_fd = -1;
  int ret = -1;

  if (parse_opts(all_options, argc, argv) < 0) {
    return -1;
  }

  if (camera_options.list_options) {
    camera = camera_open(&camera_options);
    if (camera) {
      printf("\n");
      for (int i = 0; i < MAX_DEVICES; i++) {
        device_dump_options(camera->devices[i], stdout);
      }
      camera_close(&camera);
    }
    return -1;
  }

  http_fd = http_server(&http_options, http_methods);
  if (http_fd < 0) {
    goto error;
  }

  if (rtsp_options.port > 0 && rtsp_server(&rtsp_options) < 0) {
    goto error;
  }

  webrtc_server();

  while (true) {
    camera = camera_open(&camera_options);
    if (camera) {
      ret = camera_run(camera);
      camera_close(&camera);
    }

    if (camera_options.auto_reconnect > 0) {
      LOG_INFO(NULL, "Automatically reconnecting in %d seconds...", camera_options.auto_reconnect);
      sleep(camera_options.auto_reconnect);
    } else {
      break;
    }
  }

error:
  close(http_fd);
  return ret;
}
