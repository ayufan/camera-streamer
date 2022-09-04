#include "util/http/http.h"
#include "util/opts/opts.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"
#include "output/webrtc/webrtc.h"
#include "device/camera/camera.h"
#include "output/output.h"
#include "output/rtsp/rtsp.h"

extern unsigned char html_index_html[];
extern unsigned int html_index_html_len;
extern unsigned char html_video_html[];
extern unsigned int html_video_html_len;
extern camera_t *camera;

void *camera_http_set_option(http_worker_t *worker, FILE *stream, const char *key, const char *value, void *headersp)
{
  bool *headers = headersp;

  if (!camera) {
    if (!*headers) {
      http_500(stream, "");
      *headers = true;
    }
    fprintf(stream, "No camera attached.\r\n");
    return NULL;
  }

  bool set = false;

  for (int i = 0; i < MAX_DEVICES; i++) {
    if (device_set_option_string(camera->devices[i], key, value) == 0) {
      set = true;
      break;
    }
  }

  if (set) {
    if (!*headers) {
      http_200(stream, "");
      *headers = true;
    }
    fprintf(stream, "The '%s' was set to '%s'.\r\n", key, value);
  } else {
    if (!*headers) {
      http_500(stream, "");
      *headers = true;
    }
    fprintf(stream, "Cannot set '%s' to '%s'.\r\n", key, value);
  }

  return NULL;
}

void camera_http_option(http_worker_t *worker, FILE *stream)
{
  bool headers = false;
  http_enum_params(worker, stream, camera_http_set_option, &headers);
  if (!headers) {
    http_404(stream, "");
    fprintf(stream, "No options passed.\r\n");
  }

  fprintf(stream, "\r\nSet: /option?name=value\r\n\r\n");

  if (camera) {
    for (int i = 0; i < MAX_DEVICES; i++) {
      device_dump_options(camera->devices[i], stream);
    }
  }
}

http_method_t http_methods[] = {
  { "GET /snapshot?", http_snapshot },
  { "GET /snapshot.jpg?", http_snapshot },
  { "GET /stream?", http_stream },
  { "GET /?action=snapshot", http_snapshot },
  { "GET /?action=stream", http_stream },
  { "GET /video?", http_content, "text/html", html_video_html, 0, &html_video_html_len },
  { "GET /video.h264?", http_h264_video },
  { "GET /video.mkv?", http_mkv_video },
  { "GET /video.mp4?", http_mp4_video },
  { "POST /video?", http_webrtc_offer },
  { "GET /option?", camera_http_option },
  { "GET /?", http_content, "text/html", html_index_html, 0, &html_index_html_len },
  { }
};
