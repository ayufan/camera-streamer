#pragma once

#include <stdbool.h>

struct http_worker_s;
struct buffer_s;

extern struct buffer_lock_s http_h264;
extern struct buffer_lock_s http_h264_lowres;

extern struct buffer_lock_s http_jpeg;
extern struct buffer_lock_s http_jpeg_lowres;

// M-JPEG
void http_snapshot(struct http_worker_s *worker, FILE *stream);
void http_stream(struct http_worker_s *worker, FILE *stream);
void http_option(struct http_worker_s *worker, FILE *stream);

// H264
bool h264_is_key_frame(struct buffer_s *buf);
void http_h264_video(struct http_worker_s *worker, FILE *stream);
void http_mkv_video(struct http_worker_s *worker, FILE *stream);
void http_mp4_video(struct http_worker_s *worker, FILE *stream);
void http_mov_video(struct http_worker_s *worker, FILE *stream);

#define HTTP_LOW_RES_PARAM "res=low"
