#include <stdint.h>

#ifdef USE_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>

#define FFMPEG_DATA_PACKET_EOF AVERROR_EOF
#else
#define FFMPEG_DATA_PACKET_EOF -1
#endif

typedef int (*ffmpeg_read_packet)(void *opaque, uint8_t *buf, int buf_size);
typedef int (*ffmpeg_write_packet)(void *opaque, const uint8_t *buf, int buf_size);

typedef struct ffmpeg_remuxer_s {
  const char *name;
  const char *input_format;
  const char *video_format;
  void *opaque;
  ffmpeg_read_packet read_packet;
  ffmpeg_write_packet write_packet;
  unsigned read_buffer_size;
  unsigned write_buffer_size;

  uint64_t start_time;
  unsigned frames;

#ifdef USE_FFMPEG
  AVIOContext *input_avio;
  AVFormatContext *input_context;
  AVDictionary *input_opts;
  AVIOContext *output_avio;
  AVFormatContext *output_context;
  AVPacket *packet;
  AVDictionary *output_opts;
  int video_stream;
#endif
} ffmpeg_remuxer_t;

int ffmpeg_remuxer_open(ffmpeg_remuxer_t *remuxer);
int ffmpeg_remuxer_feed(ffmpeg_remuxer_t *remuxer, int nframes);
int ffmpeg_remuxer_flush(ffmpeg_remuxer_t *remuxer);
void ffmpeg_remuxer_close(ffmpeg_remuxer_t *remuxer);
