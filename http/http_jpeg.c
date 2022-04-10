#include <stdio.h>
#include <stdlib.h>

#include "http/http.h"
#include "device/buffer.h"
#include "device/buffer_lock.h"

DEFINE_BUFFER_LOCK(http_jpeg, 1000);
DEFINE_BUFFER_LOCK(http_jpeg_lowres, 1000);

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
  return buffer_lock_needs_buffer(&http_jpeg) | buffer_lock_needs_buffer(&http_jpeg_lowres);
}

void http_jpeg_capture(buffer_t *buf)
{
  buffer_lock_capture(&http_jpeg, buf);
}

void http_jpeg_lowres_capture(buffer_t *buf)
{
  buffer_lock_capture(&http_jpeg_lowres, buf);
}

buffer_lock_t *http_jpeg_buffer_for_res(http_worker_t *worker)
{
  if (strstr(worker->client_method, HTTP_LOW_RES_PARAM))
    return &http_jpeg_lowres;
  else
    return &http_jpeg;
}

int http_snapshot_buf_part(buffer_lock_t *buf_lock, buffer_t *buf, int frame, FILE *stream)
{
  fprintf(stream, "HTTP/1.1 200 OK\r\n");
  fprintf(stream, "Content-Type: image/jpeg\r\n");
  fprintf(stream, "Content-Length: %d\r\n", buf->used);
  fprintf(stream, "\r\n");
  fwrite(buf->start, buf->used, 1, stream);
  return 1;
}

void http_snapshot(http_worker_t *worker, FILE *stream)
{
  int n = buffer_lock_write_loop(http_jpeg_buffer_for_res(worker), 1, (buffer_write_fn)http_snapshot_buf_part, stream);

  if (n <= 0) {
    http_500(stream, NULL);
    fprintf(stream, "No snapshot captured yet.\r\n");
  }
}

int http_stream_buf_part(buffer_lock_t *buf_lock, buffer_t *buf, int frame, FILE *stream)
{
  if (!frame && !fprintf(stream, STREAM_HEADER)) {
    return -1;
  }
  if (!fprintf(stream, STREAM_PART, buf->used)) {
    return -1;
  }
  if (!fwrite(buf->start, buf->used, 1, stream)) {
    return -1;
  }
  if (!fprintf(stream, STREAM_BOUNDARY)) {
    return -1;
  }

  return 1;
}

void http_stream(http_worker_t *worker, FILE *stream)
{
  int n = buffer_lock_write_loop(http_jpeg_buffer_for_res(worker), 0, (buffer_write_fn)http_stream_buf_part, stream);

  if (n == 0) {
    http_500(stream, NULL);
    fprintf(stream, "No frames.\n");
  } else if (n < 0) {
    fprintf(stream, "Interrupted. Received %d frames", -n);
  }
}
