#include <stdio.h>
#include <stdlib.h>

#include "output.h"
#include "util/opts/log.h"
#include "util/http/http.h"
#include "device/buffer.h"
#include "device/buffer_lock.h"
#include "device/buffer_list.h"
#include "device/device.h"

static const char *const CONTENT_TYPE = "application/x-mpegURL";

static const char *const STREAM_M3U8 = 
  "#EXTM3U\r\n" \
  "#EXT-X-TARGETDURATION:1\r\n" \
  "#EXT-X-VERSION:4\r\n" \
  "#EXTINF:1.0,\r\n" \
  "video.mp4?ts=%zu\r\n";

static const char *const LOCATION_REDIRECT =
  "HTTP/1.0 307 Temporary Redirect\r\n"
  "Access-Control-Allow-Origin: *\r\n"
  "Connection: close\r\n"
  "Location: %s?%s\r\n"
  "\r\n";

void http_m3u8_video(struct http_worker_s *worker, FILE *stream)
{
  uint64_t ts = get_monotonic_time_us(NULL, NULL) / 1000 / 1000;
  http_write_responsef(stream, "200 OK", CONTENT_TYPE, STREAM_M3U8, ts);
}

void http_detect_video(struct http_worker_s *worker, FILE *stream)
{
  if (strstr(worker->user_agent, "Safari/") && !strstr(worker->user_agent, "Chrome/") && !strstr(worker->user_agent, "Chromium/")) {
    // Safari only supports m3u8
    fprintf(stream, LOCATION_REDIRECT, "video.m3u8", worker->request_params);
  } else if (strstr(worker->user_agent, "Firefox/")) {
    // Firefox only supports mp4
    fprintf(stream, LOCATION_REDIRECT, "video.mp4", worker->request_params);
  } else {
    // Chrome offers best latency with mkv
    fprintf(stream, LOCATION_REDIRECT, "video.mkv", worker->request_params);
  }
}
