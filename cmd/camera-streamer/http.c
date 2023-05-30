#include "util/http/http.h"
#include "output/webrtc/webrtc.h"
#include "device/camera/camera.h"
#include "output/output.h"

extern unsigned char html_index_html[];
extern unsigned int html_index_html_len;
extern unsigned char html_webrtc_html[];
extern unsigned int html_webrtc_html_len;
extern unsigned char html_control_html[];
extern unsigned int html_control_html_len;
extern camera_t *camera;

extern void camera_status_json(http_worker_t *worker, FILE *stream);

static void http_once(FILE *stream, void (*fn)(FILE *stream, const char *data), void *headersp)
{
  bool *headers = headersp;

  if (!*headers) {
    fn(stream, "");
    *headers = true;
  }
}

static void camera_post_option(http_worker_t *worker, FILE *stream)
{
  char *device_name = http_get_param(worker, "device");
  char *key = http_get_param(worker, "key");
  char *value = http_get_param(worker, "value");

  if (!key || !value) {
    http_400(stream, "");
    fprintf(stream, "No key or value passed.\r\n");
    goto cleanup;
  }

  bool found = false;

  for (int i = 0; i < MAX_DEVICES; i++) {
    device_t *dev = camera->devices[i];
    if (!dev) {
      continue;
    }

    if (device_name && strcmp(dev->name, device_name)) {
      continue;
    }

    int ret = device_set_option_string(dev, key, value);
    if (ret > 0) {
      http_once(stream, http_200, &found);
      fprintf(stream, "%s: The '%s' was set to '%s'.\r\n", dev->name, key, value);
    } else if (ret < 0) {
      http_once(stream, http_500, &found);
      fprintf(stream, "%s: Cannot set '%s' to '%s'.\r\n", dev->name, key, value);
    }
  }

  if (!found) {
    http_once(stream, http_404, &found);
    fprintf(stream, "The option was not found for device='%s', key='%s', value='%s'.\r\n",
      device_name, key, value);
  }

cleanup:
  free(device_name);
  free(key);
  free(value);
}

static void http_cors_options(http_worker_t *worker, FILE *stream)
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
  { "GET",  "/control", http_content, "text/html", html_control_html, 0, &html_control_html_len },
  { "GET",  "/option", camera_post_option },
  { "POST", "/option", camera_post_option },
  { "GET",  "/status", camera_status_json },
  { "GET",  "/", http_content, "text/html", html_index_html, 0, &html_index_html_len },
  { "OPTIONS", "*/", http_cors_options },
  { }
};
