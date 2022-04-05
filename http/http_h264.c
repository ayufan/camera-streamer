#include <stdio.h>
#include <stdlib.h>

#include "http/http.h"
#include "hw/buffer.h"
#include "hw/buffer_lock.h"
#include "hw/buffer_list.h"
#include "hw/device.h"

DEFINE_BUFFER_LOCK(http_h264);

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

void http_video(http_worker_t *worker, FILE *stream)
{
  bool had_key_frame = false;
  bool requested_key_frame = false;
  int counter = 0;
  fprintf(stream, VIDEO_HEADER);

  buffer_lock_use(&http_h264, 1);

  while (!feof(stream)) {
    buffer_t *buf = buffer_lock_get(&http_h264, 3, &counter);
    if (!buf) {
      goto error;
    }

    if (buf->v4l2_buffer.flags & V4L2_BUF_FLAG_KEYFRAME) {
      had_key_frame = true;
      E_LOG_DEBUG(buf, "Got key frame!");
    }

    if (had_key_frame) {
      if (!fwrite(buf->start, buf->used, 1, stream)) {
        goto error;
      }
    } else if (!requested_key_frame) {
      device_force_key(buf->buf_list->device);
      requested_key_frame = true;
    }
    buffer_consumed(buf);
  }

error:
  buffer_lock_use(&http_h264, -1);
}
