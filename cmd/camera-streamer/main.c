#include "util/http/http.h"
#include "util/opts/opts.h"
#include "util/opts/log.h"
#include "device/camera/camera.h"
#include "output/rtsp/rtsp.h"
#include "output/webrtc/webrtc.h"
#include "version.h"

#include <signal.h>
#include <unistd.h>

extern option_t all_options[];
extern camera_options_t camera_options;
extern http_server_options_t http_options;
extern http_method_t http_methods[];
extern rtsp_options_t rtsp_options;
extern webrtc_options_t webrtc_options;

camera_t *camera;

void deprecations()
{
  if (camera_options.high_res_factor > 0) {
    printf("Using deprecated `-camera-high_res_factor`. Use `-camera-snapshot.height` instead.");

    if (!camera_options.snapshot.height)
      camera_options.snapshot.height = camera_options.height / camera_options.high_res_factor;
  }
  if (camera_options.low_res_factor > 0) {
    printf("Using deprecated `-camera-low_res_factor`. Use `-camera-stream.height` or `-camera-video.height` instead.");

    if (!camera_options.stream.height)
      camera_options.stream.height = camera_options.height / camera_options.low_res_factor;
    if (!camera_options.video.height)
      camera_options.video.height = camera_options.height / camera_options.low_res_factor;
  }
}

void inherit()
{
  if (!camera_options.snapshot.height || camera_options.snapshot.height > camera_options.height)
    camera_options.snapshot.height = camera_options.height;

  if (!camera_options.video.height || camera_options.video.height > camera_options.snapshot.height)
    camera_options.video.height = camera_options.snapshot.height;

  if (!camera_options.stream.height || camera_options.stream.height > camera_options.video.height)
    camera_options.stream.height = camera_options.video.height;
}

int main(int argc, char *argv[])
{
  int http_fd = -1;
  int ret = -1;

  if (parse_opts(all_options, argc, argv) < 0) {
    return -1;
  }

  printf("%s Version: %s (%s)\n", argv[0], GIT_VERSION, GIT_REVISION);

  deprecations();
  inherit();

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

  if (!webrtc_options.disabled && webrtc_server(&webrtc_options) < 0) {
    goto error;
  }

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
