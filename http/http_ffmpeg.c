#include <stdio.h>
#include <stdlib.h>

#include "http/http.h"
#include "hw/buffer.h"
#include "hw/buffer_lock.h"
#include "hw/buffer_list.h"
#include "hw/device.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>

DECLARE_BUFFER_LOCK(http_h264);

static const char *const VIDEO_HEADER =
  "HTTP/1.0 200 OK\r\n"
  "Access-Control-Allow-Origin: *\r\n"
  "Connection: close\r\n"
  "Content-Type: %s\r\n"
  "\r\n";

static AVRational time_base = {1, 1000LL * 1000LL};

typedef struct {
  const char *name;
  FILE *stream;
  const char *content_type;
  const char *video_format;

  AVIOContext *input_avio;
  AVFormatContext *input_context;
  AVIOContext *output_avio;
  AVFormatContext *output_context;
  AVPacket *packet;
  AVDictionary *output_opts;

  bool had_key_frame;
  bool requested_key_frame;
  bool wrote_header;

  uint64_t start_time;
  int video_stream;
  int pts;
  buffer_t *buf;
  unsigned buf_offset;
  unsigned stream_offset;
} http_ffmpeg_status_t;

static int http_ffmpeg_read_from_buf(void *opaque, uint8_t *buf, int buf_size)
{
  http_ffmpeg_status_t *status = opaque;
  if (!status->buf)
    return AVERROR_EOF;

  buf_size = FFMIN(buf_size, status->buf->used - status->buf_offset);
  if (!buf_size)
    return AVERROR_EOF;

  E_LOG_DEBUG(status, "http_ffmpeg_read_from_buf: offset=%d, n=%d", status->buf_offset, buf_size);
  memcpy(buf, (char*)status->buf->start + status->buf_offset, buf_size);
  status->buf_offset += buf_size;
  return buf_size;
}

static int http_ffmpeg_write_to_stream(void *opaque, uint8_t *buf, int buf_size)
{
  http_ffmpeg_status_t *status = opaque;
  if (!status->stream)
    return AVERROR_EOF;

  if (!status->wrote_header) {
    fprintf(status->stream, VIDEO_HEADER, status->content_type);
    status->wrote_header = true;
  }

  size_t n = fwrite(buf, 1, buf_size, status->stream);
  fflush(status->stream);

  E_LOG_DEBUG(status, "http_ffmpeg_write_to_stream: offset=%d, n=%d, buf_size=%d, error=%d",
    status->stream_offset, buf_size, ferror(status->stream));
  status->stream_offset += n;
  if (ferror(status->stream))
    return AVERROR_EOF;

  return n;
}

static int http_ffmpeg_init_avcontext(AVFormatContext **context, http_ffmpeg_status_t *status, int output, int (*packet)(void *opaque, uint8_t *buf, int buf_size))
{
  static int avio_ctx_buffer_size = 4096;
  uint8_t *buffer = NULL;
  AVIOContext *avio = NULL;
  int ret = -1;
  
  buffer = av_malloc(avio_ctx_buffer_size);
  if (!buffer)
    return AVERROR(ENOMEM);
  avio = avio_alloc_context(buffer, avio_ctx_buffer_size, output, status, output ? NULL : packet, output ? packet : NULL, NULL);
  if (!avio)
    goto error;
  if (output && (ret = avformat_alloc_output_context2(context, NULL, status->video_format, NULL)) < 0)
    goto error;
  if (!output && (*context = avformat_alloc_context()) == NULL)
    goto error;

  (*context)->flush_packets = true;
  (*context)->pb = avio;

  return 0;

error:
  av_freep(&buffer);
  avio_context_free(&avio);
  return -1;
}

static void http_ffmpeg_close_avcontext(AVFormatContext **context)
{
  if (!*context)
    return;

  if ((*context)->pb)
    av_freep(&(*context)->pb->buffer);
  avio_context_free(&(*context)->pb);
  avformat_close_input(context);
}

static int http_ffmpeg_copy_streams(http_ffmpeg_status_t *status)
{
  for (int i = 0; i < status->input_context->nb_streams; i++) {
    AVStream *out_stream;
    AVStream *in_stream = status->input_context->streams[i];
    AVCodecParameters *in_codecpar = in_stream->codecpar;

    if (in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
      continue;
    }

    out_stream = avformat_new_stream(status->output_context, NULL);
    if (!out_stream) {
      return AVERROR(ENOMEM);
    }

    int ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
    if (ret < 0) {
      return ret;
    }

    out_stream->time_base.num = 1;
    out_stream->time_base.den = 1;
    out_stream->codecpar->codec_tag = 0;
    status->video_stream = i;
    return 0;
  }

  return -1;
}

static int http_ffmpeg_open_status(http_ffmpeg_status_t *status)
{
  int ret;

  if (status->packet)
    return 0;
    


  status->packet = av_packet_alloc();
  if (!status->packet)
    return AVERROR(ENOMEM);
  if ((ret = http_ffmpeg_init_avcontext(&status->input_context, status, 0, http_ffmpeg_read_from_buf)) < 0)
    return ret;
  if ((ret = http_ffmpeg_init_avcontext(&status->output_context, status, 1, http_ffmpeg_write_to_stream)) < 0)
    return ret;
  if ((ret = avformat_open_input(&status->input_context, NULL, NULL, NULL)) < 0)
    return ret;
  if ((ret = avformat_find_stream_info(status->input_context, NULL)) < 0)
    return ret;
  if ((ret = http_ffmpeg_copy_streams(status)) < 0)
    return ret;
  if ((ret = avformat_write_header(status->output_context, &status->output_opts)) < 0)
    return ret;

  status->start_time = get_monotonic_time_us(NULL, NULL);

  return 0;
}

static int http_ffmpeg_close_status(http_ffmpeg_status_t *status)
{
  http_ffmpeg_close_avcontext(&status->input_context);
  http_ffmpeg_close_avcontext(&status->output_context);
  av_packet_free(&status->packet);
  av_dict_free(&status->output_opts);
}

static int http_ffmpeg_copy_packets(http_ffmpeg_status_t *status)
{
  int ret = 0;

  while (ret >= 0) {
    ret = av_read_frame(status->input_context, status->packet);
    if (ret == AVERROR_EOF) {
      ret = 0;
      E_LOG_DEBUG(status, "av_read_frame: EOF", ret, status->pts);
      break;
    } else if (ret < 0) {
      E_LOG_DEBUG(status, "av_read_frame: %08x, pts: %d", ret, status->packet->pts);
      break;
    }

    if (status->packet->stream_index != status->video_stream) {
      av_packet_unref(status->packet);
      continue;
    }

    AVStream *in_stream = status->input_context->streams[status->packet->stream_index];
    status->packet->stream_index = 0;
    AVStream *out_stream = status->output_context->streams[status->packet->stream_index];

    int since_start = get_monotonic_time_us(NULL, NULL) - status->start_time;

    // TODO: fix a PTS to be valid
    status->packet->pos = -1;
    int pts = status->packet->dts = status->packet->pts = av_rescale_q(
      get_monotonic_time_us(NULL, NULL) - status->start_time,
      time_base,
      out_stream->time_base
    );
 
    ret = av_interleaved_write_frame(status->output_context, status->packet);

    if (ret == AVERROR_EOF) {
      E_LOG_DEBUG(status, "av_interleaved_write_frame: EOF, pts: %d, since_start: %d", ret, pts, since_start);
    } else {
      E_LOG_DEBUG(status, "av_interleaved_write_frame: %08x, pts: %d, since_start: %d", ret, pts, since_start);
    }
  }

  return ret;
}

static int http_ffmpeg_video_buf_part(buffer_lock_t *buf_lock, buffer_t *buf, int frame, http_ffmpeg_status_t *status)
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

  int ret = -1;

  status->buf = buf;
  status->buf_offset = 0;

  if ((ret = http_ffmpeg_open_status(status)) < 0)
    goto error;
  if ((ret = http_ffmpeg_copy_packets(status)) < 0)
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
    .video_format = video_format
  };

  av_dict_set_int(&status.output_opts, "probesize", 4096, 0);
  av_dict_set_int(&status.output_opts, "direct", 1, 0);
  //av_dict_set_int(&status.output_opts, "frag_duration", 1, 0);
  av_dict_set_int(&status.output_opts, "frag_size", 4096, 0);
  av_dict_set_int(&status.output_opts, "low_delay", 1, 0);
  av_dict_set_int(&status.output_opts, "nobuffer", 1, 0);
  av_dict_set_int(&status.output_opts, "flush_packets", 1, 0);

  int n = buffer_lock_write_loop(&http_h264, 0, (buffer_write_fn)http_ffmpeg_video_buf_part, &status);
  http_ffmpeg_close_status(&status);

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

void http_mkv_video(http_worker_t *worker, FILE *stream)
{
  http_ffmpeg_video(worker, stream, "video/mp4", "matroska");
}

void http_mp4_video(http_worker_t *worker, FILE *stream)
{
  http_ffmpeg_video(worker, stream, "video/mp4", "mp4");
}
