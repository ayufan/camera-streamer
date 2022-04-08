#ifdef USE_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>

#define FFMPEG_DATA_PACKET_EOF AVERROR_EOF
#else
#define FFMPEG_DATA_PACKET_EOF -1
#endif

typedef int (*ffmpeg_data_packet)(void *opaque, uint8_t *buf, int buf_size);

typedef struct ffmpeg_remuxer_s {
  const char *name;
  const char *input_format;
  const char *video_format;
  void *opaque;
  ffmpeg_data_packet read_packet;
  ffmpeg_data_packet write_packet;

#ifdef USE_FFMPEG
  AVIOContext *input_avio;
  AVFormatContext *input_context;
  AVDictionary *input_opts;
  AVIOContext *output_avio;
  AVFormatContext *output_context;
  AVPacket *packet;
  AVDictionary *output_opts;
  int video_stream;
  uint64_t start_time;
#endif
} ffmpeg_remuxer_t;

int ffmpeg_remuxer_open(ffmpeg_remuxer_t *remuxer);
int ffmpeg_remuxer_feed(ffmpeg_remuxer_t *remuxer);
int ffmpeg_remuxer_close(ffmpeg_remuxer_t *remuxer);
