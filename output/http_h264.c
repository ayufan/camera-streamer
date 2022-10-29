#include <stdio.h>
#include <stdlib.h>

#include "output.h"
#include "util/opts/log.h"
#include "util/http/http.h"
#include "device/buffer.h"
#include "device/buffer_lock.h"
#include "device/buffer_list.h"
#include "device/device.h"

static const char *const VIDEO_HEADER =
  "HTTP/1.0 200 OK\r\n"
  "Access-Control-Allow-Origin: *\r\n"
  "Connection: close\r\n"
  "Content-Type: application/octet-stream\r\n"
  "\r\n";

typedef struct {
  FILE *stream;
  bool wrote_header;
  bool had_key_frame;
  bool requested_key_frame;
} http_video_status_t;

int http_video_buf_part(buffer_lock_t *buf_lock, buffer_t *buf, int frame, http_video_status_t *status)
{
  if (!status->had_key_frame) {
    status->had_key_frame = buf->flags.is_keyframe;
  }

  if (!status->had_key_frame) {
    if (!status->requested_key_frame) {
      device_video_force_key(buf->buf_list->dev);
      status->requested_key_frame = true;
    }
    return 0;
  }

  if (!status->wrote_header) {
    fputs(VIDEO_HEADER, status->stream);
    status->wrote_header = true;
  }
  if (!fwrite(buf->start, buf->used, 1, status->stream)) {
    return -1;
  }
  fflush(status->stream);
  return 1;
}

void http_h264_video(http_worker_t *worker, FILE *stream)
{
  http_video_status_t status = { stream };

  int n = buffer_lock_write_loop(&video_lock, 0, 0, (buffer_write_fn)http_video_buf_part, &status);

  if (status.wrote_header) {
    return;
  }

  http_500(stream, NULL);

  if (n == 0) {
    fprintf(stream, "No frames.\n");
  } else if (n < 0) {
    fprintf(stream, "Interrupted. Received %d frames", -n);
  }
}
