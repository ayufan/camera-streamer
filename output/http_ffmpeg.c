#include <stdio.h>
#include <stdlib.h>

#include "output.h"
#include "util/opts/log.h"
#include "util/http/http.h"
#include "device/buffer.h"
#include "device/buffer_lock.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "util/ffmpeg/remuxer.h"

static const char *const VIDEO_HEADER =
  "HTTP/1.0 200 OK\r\n"
  "Access-Control-Allow-Origin: *\r\n"
  "Connection: close\r\n"
  "Content-Type: %s\r\n"
  "\r\n";

typedef struct {
  const char *name;
  FILE *stream;
  const char *content_type;

  bool had_key_frame;
  bool requested_key_frame;
  bool wrote_header;

  buffer_t *buf;
  unsigned buf_offset;
  unsigned stream_offset;

  ffmpeg_remuxer_t *remuxer;
} http_ffmpeg_status_t;

static int http_ffmpeg_read_from_buf(void *opaque, uint8_t *buf, int buf_size)
{
  http_ffmpeg_status_t *status = opaque;
  if (!status->buf)
    return FFMPEG_DATA_PACKET_EOF;

  buf_size = MIN(buf_size, status->buf->used - status->buf_offset);
  if (!buf_size)
    return FFMPEG_DATA_PACKET_EOF;

  LOG_DEBUG(status, "http_ffmpeg_read_from_buf: offset=%d, n=%d", status->buf_offset, buf_size);
  memcpy(buf, (char*)status->buf->start + status->buf_offset, buf_size);
  status->buf_offset += buf_size;
  return buf_size;
}

static int http_ffmpeg_write_to_stream(void *opaque, const uint8_t *buf, int buf_size)
{
  http_ffmpeg_status_t *status = opaque;
  if (!status->stream)
    return FFMPEG_DATA_PACKET_EOF;

  if (!status->wrote_header) {
    fprintf(status->stream, VIDEO_HEADER, status->content_type);
    status->wrote_header = true;
  }

  size_t n = fwrite(buf, 1, buf_size, status->stream);
  fflush(status->stream);

  LOG_DEBUG(status, "http_ffmpeg_write_to_stream: offset=%d, n=%zu, buf_size=%d, error=%d",
    status->stream_offset, n, buf_size, ferror(status->stream));
  status->stream_offset += n;
  if (ferror(status->stream))
    return FFMPEG_DATA_PACKET_EOF;

  return n;
}

static int http_ffmpeg_video_buf_part(buffer_lock_t *buf_lock, buffer_t *buf, int frame, http_ffmpeg_status_t *status)
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

  int ret = -1;

  status->buf = buf;
  status->buf_offset = 0;

  if ((ret = ffmpeg_remuxer_open(status->remuxer)) < 0)
    goto error;
  if ((ret = ffmpeg_remuxer_feed(status->remuxer, 0)) < 0)
    goto error;

  ret = 1;

error:
  status->buf = NULL;
  return ret;
}

static void http_ffmpeg_video(http_worker_t *worker, FILE *stream, const char *content_type, const char *video_format)
{
  http_ffmpeg_status_t status = {
    .name = worker->name,
    .stream = stream,
    .content_type = content_type,
  };

  ffmpeg_remuxer_t remuxer = {
    .name = worker->name,
    .input_format = "h264",
    .video_format = video_format,
    .opaque = &status,
    .read_packet = http_ffmpeg_read_from_buf,
    .write_packet = http_ffmpeg_write_to_stream,
  };

  status.remuxer = &remuxer;

#ifdef USE_FFMPEG
  av_dict_set_int(&remuxer.output_opts, "direct", 1, 0);
  //av_dict_set_int(&remuxer.output_opts, "frag_duration", 1, 0);
  av_dict_set_int(&remuxer.output_opts, "frag_size", 4096, 0);
  av_dict_set_int(&remuxer.output_opts, "low_delay", 1, 0);
  av_dict_set_int(&remuxer.output_opts, "nobuffer", 1, 0);
  av_dict_set_int(&remuxer.output_opts, "flush_packets", 1, 0);
  av_dict_set(&remuxer.output_opts, "movflags", "frag_keyframe+empty_moov+default_base_moof", 0);
#endif

  int n = buffer_lock_write_loop(
    &video_lock,
    0,
    0,
    (buffer_write_fn)http_ffmpeg_video_buf_part,
    &status);
  ffmpeg_remuxer_close(&remuxer);

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

void http_mkv_video(http_worker_t *worker, FILE *stream)
{
  http_ffmpeg_video(worker, stream, "video/mp4", "matroska");
}

void http_mp4_video(http_worker_t *worker, FILE *stream)
{
  http_ffmpeg_video(worker, stream, "video/mp4", "mp4");
}

