#include <stdio.h>
#include <stdlib.h>

#include "http/http.h"
#include "hw/buffer.h"
#include "hw/buffer_lock.h"
#include "hw/buffer_list.h"
#include "hw/device.h"

DEFINE_BUFFER_LOCK(http_h264, 0);

static const char *const VIDEO_HEADER = "HTTP/1.0 200 OK\r\n"
                                         "Access-Control-Allow-Origin: *\r\n"
                                         "Connection: close\r\n"
                                         "Content-Type: video/webm;codecs=h264\r\n"
                                         "\r\n";

void http_h264_capture(buffer_t *buf)
{
  buffer_lock_capture(&http_h264, buf);
}

bool http_h264_needs_buffer()
{
  return buffer_lock_needs_buffer(&http_h264);
}

typedef struct {
  FILE *stream;
  bool wrote_header;
  bool had_key_frame;
  bool requested_key_frame;
} http_video_status_t;

int http_video_buf_part(buffer_lock_t *buf_lock, buffer_t *buf, int frame, http_video_status_t *status)
{
  unsigned char *data = buf->start;

  if (buf->v4l2_buffer.flags & V4L2_BUF_FLAG_KEYFRAME) {
    status->had_key_frame = true;
    E_LOG_DEBUG(buf, "Got key frame (from V4L2)!");
  } else if (buf->used >= 5 && (data[4] & 0x1F) == 0x07) {
    status->had_key_frame = true;
    E_LOG_DEBUG(buf, "Got key frame (from buffer)!");
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

void http_video(http_worker_t *worker, FILE *stream)
{
  http_video_status_t status = { stream };

  int n = buffer_lock_write_loop(&http_h264, 0, (buffer_write_fn)http_video_buf_part, &status);

  if (status.wrote_header) {
    return;
  }

  http_500_header(stream);

  if (n == 0) {
    fprintf(stream, "No frames.\n");
  } else if (n < 0) {
    fprintf(stream, "Interrupted. Received %d frames", -n);
  }
}
