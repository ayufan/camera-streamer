#pragma once

#include <stdbool.h>

struct http_worker_s;
struct buffer_s;

extern struct buffer_lock_s snapshot_lock;
extern struct buffer_lock_s stream_lock;
extern struct buffer_lock_s video_lock;

// M-JPEG
void http_snapshot(struct http_worker_s *worker, FILE *stream);
void http_stream(struct http_worker_s *worker, FILE *stream);
void http_option(struct http_worker_s *worker, FILE *stream);

// H264
void http_h264_video(struct http_worker_s *worker, FILE *stream);
void http_mkv_video(struct http_worker_s *worker, FILE *stream);
void http_mp4_video(struct http_worker_s *worker, FILE *stream);
void http_mov_video(struct http_worker_s *worker, FILE *stream);

// HLS
void http_m3u8_video(struct http_worker_s *worker, FILE *stream);
void http_detect_video(struct http_worker_s *worker, FILE *stream);

#define HTTP_LOW_RES_PARAM "res=low"
