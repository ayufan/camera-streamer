#include <stdio.h>
#include <stdlib.h>

#include "output.h"
#include "util/http/http.h"
#include "util/opts/log.h"
#include "device/buffer.h"
#include "device/buffer_lock.h"

#define SNAPSHOT_TIMEOUT_MS 3000
#define SNAPSHOT_DEFAULT_DELAY_PARAM 300

#define PART_BOUNDARY "123456789000000000000987654321"
#define CONTENT_TYPE "image/jpeg"
#define CONTENT_LENGTH "Content-Length"

static const char *const STREAM_HEADER = "HTTP/1.0 200 OK\r\n"
                                         "Access-Control-Allow-Origin: *\r\n"
                                         "Connection: close\r\n"
                                         "Content-Type: multipart/x-mixed-replace;boundary=" PART_BOUNDARY "\r\n"
                                         "\r\n"
                                         "--" PART_BOUNDARY "\r\n";
static const char *const STREAM_PART = "Content-Type: " CONTENT_TYPE "\r\n" CONTENT_LENGTH ": %u\r\n\r\n";
static const char *const STREAM_BOUNDARY = "\r\n"
                                           "--" PART_BOUNDARY "\r\n";

typedef struct
{
  FILE *stream;
  uint64_t start_time_us;
} http_snapshot_t;

int http_snapshot_buf_part(buffer_lock_t *buf_lock, buffer_t *buf, int frame, http_snapshot_t *snapshot)
{
  // Ignore frames that are captured
  if (buf->captured_time_us < snapshot->start_time_us) {
    return 0;
  }

  fprintf(snapshot->stream, "HTTP/1.1 200 OK\r\n");
  fprintf(snapshot->stream, "Content-Type: image/jpeg\r\n");
  fprintf(snapshot->stream, "Content-Length: %zu\r\n", buf->used);
  fprintf(snapshot->stream, "\r\n");
  fwrite(buf->start, buf->used, 1, snapshot->stream);
  return 1;
}

void http_snapshot(http_worker_t *worker, FILE *stream)
{
  int max_delay_value = SNAPSHOT_DEFAULT_DELAY_PARAM;

  // passing the max_delay=0 will ensure that frame is capture at this exact moment
  char *max_delay = http_get_param(worker, "max_delay");
  if (max_delay) {
    max_delay_value = atoi(max_delay);
    free(max_delay);
  }

  http_snapshot_t snapshot = {
    .stream = stream,
    .start_time_us = get_monotonic_time_us(NULL, NULL) - max_delay_value * 1000
  };

  int n = buffer_lock_write_loop(&snapshot_lock, 1, SNAPSHOT_TIMEOUT_MS,
    (buffer_write_fn)http_snapshot_buf_part, &snapshot);

  if (n <= 0) {
    http_500(stream, NULL);
    fprintf(stream, "No snapshot captured yet.\r\n");
  }
}

int http_stream_buf_part(buffer_lock_t *buf_lock, buffer_t *buf, int frame, FILE *stream)
{
  if (!frame && !fputs(STREAM_HEADER, stream)) {
    return -1;
  }
  if (!fprintf(stream, STREAM_PART, buf->used)) {
    return -1;
  }
  if (!fwrite(buf->start, buf->used, 1, stream)) {
    return -1;
  }
  if (!fputs(STREAM_BOUNDARY, stream)) {
    return -1;
  }

  return 1;
}

void http_stream(http_worker_t *worker, FILE *stream)
{
  int n = buffer_lock_write_loop(&stream_lock, 0, 0, (buffer_write_fn)http_stream_buf_part, stream);

  if (n == 0) {
    http_500(stream, NULL);
    fprintf(stream, "No frames.\n");
  } else if (n < 0) {
    fprintf(stream, "Interrupted. Received %d frames", -n);
  }
}
