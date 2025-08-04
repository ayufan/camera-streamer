#include "remuxer.h"
#include "util/opts/log.h"

#include <inttypes.h>

#ifdef USE_FFMPEG
static AVRational time_base = {1, 1000LL * 1000LL};
static unsigned avio_ctx_buffer_size = 4096;

#if LIBAVFORMAT_VERSION_MAJOR < 61
typedef int (*ffmpeg_write_nonconst_packet)(void *opaque, uint8_t *buf, int buf_size);
#else // LIBAVFORMAT_VERSION_MAJOR < 61
#define ffmpeg_write_nonconst_packet ffmpeg_write_packet
#endif // LIBAVFORMAT_VERSION_MAJOR < 61

static int ffmpeg_remuxer_init_avcontext(AVFormatContext **context, ffmpeg_remuxer_t *remuxer, int output, ffmpeg_write_packet packet_out, ffmpeg_read_packet packet_in)
{
  uint8_t *buffer = NULL;
  AVIOContext *avio = NULL;
  int ret = -1;

  unsigned buffer_size = MAX(
    output ? remuxer->write_buffer_size : remuxer->read_buffer_size,
    avio_ctx_buffer_size);
  
  buffer = av_malloc(buffer_size);
  if (!buffer)
    return AVERROR(ENOMEM);
  avio = avio_alloc_context(buffer, buffer_size, output, remuxer->opaque, packet_in, (ffmpeg_write_nonconst_packet)packet_out, NULL);
  if (!avio)
    goto error;
  if (output && (ret = avformat_alloc_output_context2(context, NULL, remuxer->video_format, NULL)) < 0)
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

static void ffmpeg_remuxer_close_avcontext(AVFormatContext **contextp)
{
  if (!*contextp)
    return;

  AVFormatContext *context = *contextp;

  if (context->pb)
    av_freep(&context->pb->buffer);
  avio_context_free(&context->pb);
  if (context->iformat)
    avformat_close_input(contextp);
  else
    avformat_free_context(context);
  *contextp = NULL;
}

static int ffmpeg_remuxer_copy_streams(ffmpeg_remuxer_t *remuxer)
{
  for (int i = 0; i < remuxer->input_context->nb_streams; i++) {
    AVStream *out_stream;
    AVStream *in_stream = remuxer->input_context->streams[i];
    AVCodecParameters *in_codecpar = in_stream->codecpar;

    if (in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
      continue;
    }

    out_stream = avformat_new_stream(remuxer->output_context, NULL);
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
    remuxer->video_stream = i;
    return 0;
  }

  return -1;
}

int ffmpeg_remuxer_open(ffmpeg_remuxer_t *remuxer)
{
  int ret;

  if (remuxer->packet)
    return 0;

#if LIBAVFORMAT_VERSION_MAJOR < 59
  AVInputFormat *input_format;
#else
  const AVInputFormat *input_format;
#endif
  input_format = av_find_input_format(remuxer->input_format);
  if (!input_format)
    return AVERROR(EINVAL);

  remuxer->packet = av_packet_alloc();
  if (!remuxer->packet)
    return AVERROR(ENOMEM);
  if ((ret = ffmpeg_remuxer_init_avcontext(&remuxer->input_context, remuxer, 0, NULL, remuxer->read_packet)) < 0)
    return ret;
  if ((ret = ffmpeg_remuxer_init_avcontext(&remuxer->output_context, remuxer, 1, remuxer->write_packet, NULL)) < 0)
    return ret;
  if ((ret = avformat_open_input(&remuxer->input_context, NULL, input_format, &remuxer->input_opts)) < 0)
    return ret;
  if ((ret = avformat_find_stream_info(remuxer->input_context, NULL)) < 0)
    return ret;
  if ((ret = ffmpeg_remuxer_copy_streams(remuxer)) < 0)
    return ret;
  if ((ret = avformat_write_header(remuxer->output_context, &remuxer->output_opts)) < 0)
    return ret;

  remuxer->start_time = get_monotonic_time_us(NULL, NULL);
  return 0;
}

void ffmpeg_remuxer_close(ffmpeg_remuxer_t *remuxer)
{
  if (remuxer->output_context)
    av_write_trailer(remuxer->output_context);
  ffmpeg_remuxer_close_avcontext(&remuxer->input_context);
  ffmpeg_remuxer_close_avcontext(&remuxer->output_context);
  av_packet_free(&remuxer->packet);
  av_dict_free(&remuxer->input_opts);
  av_dict_free(&remuxer->output_opts);
}

int ffmpeg_remuxer_feed(ffmpeg_remuxer_t *remuxer, int nframes)
{
  int ret = 0;
  int frames = 0;

  while (ret >= 0) {
    if (nframes > 0 && frames >= nframes) {
      break;
    }

    ret = av_read_frame(remuxer->input_context, remuxer->packet);
    if (ret == AVERROR_EOF) {
      ret = 0;
      LOG_DEBUG(remuxer, "av_read_frame: EOF");
      break;
    } else if (ret < 0) {
      LOG_DEBUG(remuxer, "av_read_frame: %08x, pts: %" PRId64, ret, remuxer->packet->pts);
      break;
    }

    if (remuxer->packet->stream_index != remuxer->video_stream) {
      av_packet_unref(remuxer->packet);
      continue;
    }

    remuxer->frames++;
    frames++;

    remuxer->packet->stream_index = 0;
    AVStream *out_stream = remuxer->output_context->streams[remuxer->packet->stream_index];

    int since_start = get_monotonic_time_us(NULL, NULL) - remuxer->start_time;

    // TODO: fix a PTS to be valid
    remuxer->packet->pos = -1;
    int64_t pts = remuxer->packet->dts = remuxer->packet->pts = av_rescale_q(
      get_monotonic_time_us(NULL, NULL) - remuxer->start_time,
      time_base,
      out_stream->time_base
    );
 
    ret = av_interleaved_write_frame(remuxer->output_context, remuxer->packet);
    av_packet_unref(remuxer->packet);

    if (ret == AVERROR_EOF) {
      LOG_DEBUG(remuxer, "av_interleaved_write_frame: EOF, pts: %" PRId64 ", since_start: %d", pts, since_start);
    } else {
      LOG_DEBUG(remuxer, "av_interleaved_write_frame: %08x, pts: %" PRId64 ", since_start: %d", ret, pts, since_start);
    }
  }

  if (ret >= 0) {
    return frames;
  }

  return ret;
}

int ffmpeg_remuxer_flush(ffmpeg_remuxer_t *remuxer)
{
  int ret = av_write_frame(remuxer->output_context, NULL);
  if (ret == AVERROR_EOF) {
    LOG_DEBUG(remuxer, "av_write_frame (flush): EOF");
  } else {
    LOG_DEBUG(remuxer, "av_write_frame (flush): %08x", ret);
  }

  return ret;
}
#else
int ffmpeg_remuxer_open(ffmpeg_remuxer_t *remuxer)
{
  return -1;
}

int ffmpeg_remuxer_feed(ffmpeg_remuxer_t *remuxer, int nframes)
{
  return -1;
}

int ffmpeg_remuxer_flush(ffmpeg_remuxer_t *remuxer)
{
  return -1;
}

void ffmpeg_remuxer_close(ffmpeg_remuxer_t *remuxer)
{
}
#endif
