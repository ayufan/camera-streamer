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
extern unsigned char html_webrtc_html[];
extern unsigned int html_webrtc_html_len;
extern camera_t *camera;

static void http_once(FILE *stream, void (*fn)(FILE *stream, const char *data), void *headersp)
{
  bool *headers = headersp;

  if (!*headers) {
    fn(stream, "");
    *headers = true;
  }
}

void *camera_http_set_option(http_worker_t *worker, FILE *stream, const char *key, const char *value, void *headersp)
{
  if (!camera) {
    http_once(stream, http_500, headersp);
    fprintf(stream, "No camera attached.\r\n");
    return NULL;
  }

  bool found = false;

  for (int i = 0; i < MAX_DEVICES; i++) {
    device_t *dev = camera->devices[i];
    if (!dev) {
      continue;
    }

    int ret = device_set_option_string(dev, key, value);
    if (ret > 0) {
      http_once(stream, http_200, headersp);
      fprintf(stream, "%s: The '%s' was set to '%s'.\r\n", dev->name, key, value);
    } else if (ret < 0) {
      http_once(stream, http_500, headersp);
      fprintf(stream, "%s: Cannot set '%s' to '%s'.\r\n", dev->name, key, value);
    }
    found = true;
  }

  if (found)
    return NULL;

  http_once(stream, http_404, headersp);
  fprintf(stream, "The '%s' was set not found.\r\n", key);
  return NULL;
}

void camera_http_option(http_worker_t *worker, FILE *stream)
{
  bool headers = false;
  http_enum_params(worker, stream, camera_http_set_option, &headers);
  if (headers) {
    fprintf(stream, "---\r\n");
  } else {
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

void http_cors_options(http_worker_t *worker, FILE *stream)
{
  fprintf(stream, "HTTP/1.1 204 No Data\r\n");
  fprintf(stream, "Access-Control-Allow-Origin: *\r\n");
  fprintf(stream, "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
  fprintf(stream, "Access-Control-Allow-Headers: Content-Type\r\n");
  fprintf(stream, "\r\n");
}

http_method_t http_methods[] = {
  { "GET",  "/snapshot", http_snapshot },
  { "GET",  "/snapshot.jpg", http_snapshot },
  { "GET",  "/stream", http_stream },
  { "GET",  "/?action=snapshot", http_snapshot },
  { "GET",  "/?action=stream", http_stream },
  { "GET",  "/video", http_detect_video },
  { "GET",  "/video.m3u8", http_m3u8_video },
  { "GET",  "/video.h264", http_h264_video },
  { "GET",  "/video.mkv", http_mkv_video },
  { "GET",  "/video.mp4", http_mp4_video },
  { "GET",  "/webrtc", http_content, "text/html", html_webrtc_html, 0, &html_webrtc_html_len },
  { "POST", "/webrtc", http_webrtc_offer },
  { "GET",  "/option", camera_http_option },
  { "GET",  "/", http_content, "text/html", html_index_html, 0, &html_index_html_len },
  { "OPTIONS", "*/", http_cors_options },
  { }
};
