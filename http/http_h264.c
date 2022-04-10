#include <stdio.h>
#include <stdlib.h>

#include "opts/log.h"
#include "http/http.h"
#include "device/buffer.h"
#include "device/buffer_lock.h"
#include "device/hw/buffer_list.h"
#include "device/hw/device.h"

DEFINE_BUFFER_LOCK(http_h264, 0);
DEFINE_BUFFER_LOCK(http_h264_lowres, 0);

static const char *const VIDEO_HEADER =
  "HTTP/1.0 200 OK\r\n"
  "Access-Control-Allow-Origin: *\r\n"
  "Connection: close\r\n"
  "Content-Type: application/octet-stream\r\n"
  "\r\n";

void http_h264_capture(buffer_t *buf)
{
  buffer_lock_capture(&http_h264, buf);
}

void http_h264_lowres_capture(buffer_t *buf)
{
  buffer_lock_capture(&http_h264_lowres, buf);
}

bool http_h264_needs_buffer()
{
  return buffer_lock_needs_buffer(&http_h264) | buffer_lock_needs_buffer(&http_h264_lowres);
}

buffer_lock_t *http_h264_buffer_for_res(http_worker_t *worker)
{
  if (strstr(worker->client_method, HTTP_LOW_RES_PARAM))
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
  device_t *camera = buf->buf_list->device;

  if (buf->v4l2.flags & V4L2_BUF_FLAG_KEYFRAME) {
    E_LOG_DEBUG(buf, "Got key frame (from V4L2)!");
    return true;
  } else if (buf->used >= 5 && (data[4] & 0x1F) == 0x07) {
    E_LOG_DEBUG(buf, "Got key frame (from buffer)!");
    return true;
  }

  return false;
}

int http_video_buf_part(buffer_lock_t *buf_lock, buffer_t *buf, int frame, http_video_status_t *status)
{
  unsigned char *data = buf->start;

  if (!status->had_key_frame) {
    status->had_key_frame = h264_is_key_frame(buf);
  }

  if (!status->had_key_frame) {
    if (!status->requested_key_frame) {
      device_video_force_key(buf->buf_list->device);
      status->requested_key_frame = true;
    }
    return 0;
  }

  if (!status->wrote_header) {
    fprintf(status->stream, VIDEO_HEADER);
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
