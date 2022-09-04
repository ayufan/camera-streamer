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

buffer_lock_t *http_h264_buffer_for_res(http_worker_t *worker)
{
  if (strstr(worker->client_method, HTTP_LOW_RES_PARAM) && http_jpeg_lowres.buf_list)
    return &http_h264_lowres;
  else
    return &http_h264;
}

typedef struct {
  FILE *stream;
  bool wrote_header;
  bool had_key_frame;
  bool requested_key_frame;
} http_video_status_t;

bool h264_is_key_frame(buffer_t *buf)
{
  unsigned char *data = buf->start;

  static const int N = 8;
  char buffer [3*N+1];
  buffer[sizeof(buffer)-1] = 0;
  for(int j = 0; j < N; j++)
    sprintf(&buffer[sizeof(buffer)/N*j], "%02X ", data[j]);

  if (buf->flags.is_keyframe) {
    LOG_DEBUG(buf, "Got key frame (from V4L2)!: %s", buffer);
    return true;
  } else if (buf->used >= 5 && (data[4] & 0x1F) == 0x07) {
    LOG_DEBUG(buf, "Got key frame (from buffer)!: %s", buffer);
    return true;
  }

  return false;
}

int http_video_buf_part(buffer_lock_t *buf_lock, buffer_t *buf, int frame, http_video_status_t *status)
{
  if (!status->had_key_frame) {
    status->had_key_frame = h264_is_key_frame(buf);
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

  int n = buffer_lock_write_loop(http_h264_buffer_for_res(worker), 0, (buffer_write_fn)http_video_buf_part, &status);

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
