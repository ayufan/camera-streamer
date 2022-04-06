#include <stdio.h>
#include <stdlib.h>

#include "http/http.h"
#include "hw/buffer.h"
#include "hw/buffer_lock.h"

DEFINE_BUFFER_LOCK(http_jpeg, 1000);

#define PART_BOUNDARY "123456789000000000000987654321"
#define CONTENT_TYPE "image/jpeg"
#define CONTENT_LENGTH "Content-Length"

static const char *const STREAM_HEADER = "HTTP/1.0 200 OK\r\n"
                                         "Access-Control-Allow-Origin: *\r\n"
                                         "Connection: close\r\n"
                                         "Content-Type: multipart/x-mixed-replace;boundary=" PART_BOUNDARY "\r\n"
                                         "\r\n"
                                         "--" PART_BOUNDARY "\r\n";
static const char *const STREAM_ERROR = "Content-Type: text/plain\r\n"
                                        "\r\n"
                                        "Error: %d (%s).\r\n"
                                        "--" PART_BOUNDARY "\r\n";
static const char *const STREAM_TIMEDOUT = "Content-Type: text/plain\r\n"
                                        "\r\n"
                                        "Timedout.\r\n"
                                        "--" PART_BOUNDARY "\r\n";
static const char *const STREAM_PART = "Content-Type: " CONTENT_TYPE "\r\n" CONTENT_LENGTH ": %u\r\n\r\n";
static const char *const STREAM_BOUNDARY = "\r\n"
                                           "--" PART_BOUNDARY "\r\n";

bool http_jpeg_needs_buffer()
{
  return buffer_lock_needs_buffer(&http_jpeg);
}

void http_jpeg_capture(buffer_t *buf)
{
  buffer_lock_capture(&http_jpeg, buf);
}

void http_snapshot(http_worker_t *worker, FILE *stream)
{
  int counter = 0;
  buffer_lock_use(&http_jpeg, 1);
  buffer_t *buf = buffer_lock_get(&http_jpeg, 1, &counter);

  if (!buf) {
    http_404_header(worker, stream);
    fprintf(stream, "No snapshot captured yet.\r\n");
    goto error;
  }

  fprintf(stream, "HTTP/1.1 200 OK\r\n");
  fprintf(stream, "Content-Type: image/jpeg\r\n");
  fprintf(stream, "Content-Length: %d\r\n", buf->used);
  fprintf(stream, "\r\n");
  fwrite(buf->start, buf->used, 1, stream);
  buffer_consumed(buf, "jpeg-snapshot");
error:
  buffer_lock_use(&http_jpeg, -1);
}

void http_stream(http_worker_t *worker, FILE *stream)
{
  int counter = 0;
  fprintf(stream, STREAM_HEADER);
  buffer_lock_use(&http_jpeg, 1);

  while (!feof(stream)) {
    buffer_t *buf = buffer_lock_get(&http_jpeg, 3, &counter);

    if (!buf) {
      fprintf(stream, STREAM_ERROR, -1, "No frames.");
      goto error;
    }

    if (!fprintf(stream, STREAM_PART, buf->used)) {
      goto error;
    }
    if (!fwrite(buf->start, buf->used, 1, stream)) {
      goto error;
    }
    if (!fprintf(stream, STREAM_BOUNDARY)) {
      goto error;
    }

    buffer_consumed(buf, "jpeg-stream");
  }

error:
  buffer_lock_use(&http_jpeg, -1);
}
