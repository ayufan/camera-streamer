#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>

typedef struct buffer_s buffer_t;
typedef struct http_worker_s http_worker_t;

typedef void (*http_method_fn)(struct http_worker_s *worker, FILE *stream);

typedef struct http_method_s {
  const char *name;
  http_method_fn func;
} http_method_t;

typedef struct http_worker_s {
  char *name;
  int listen_fd;
  http_method_t *methods;
  pthread_t thread;

  int client_fd;
  struct sockaddr_in client_addr;
  char *client_host;
  char client_method[256];
} http_worker_t;

typedef struct http_server_options_s {
  int port;
  int maxcons;
} http_server_options_t;

int http_server(http_server_options_t *options, http_method_t *methods);

void http_index(http_worker_t *worker, FILE *stream);
void http_video_html(http_worker_t *worker, FILE *stream);
void http_jmuxer_js(http_worker_t *worker, FILE *stream);
void http_custom_header(FILE *stream, const char *status);
void http_404_header(FILE *stream);
void http_500_header(FILE *stream);
void http_404(http_worker_t *worker, FILE *stream);

// M-JPEG
void http_snapshot(http_worker_t *worker, FILE *stream);
void http_stream(http_worker_t *worker, FILE *stream);
void http_jpeg_capture(struct buffer_s *buf);
void http_jpeg_lowres_capture(struct buffer_s *buf);
bool http_jpeg_needs_buffer();

// H264
bool http_h264_needs_buffer();
void http_h264_capture(buffer_t *buf);
void http_h264_lowres_capture(buffer_t *buf);
void http_h264_video(http_worker_t *worker, FILE *stream);
bool h264_is_key_frame(buffer_t *buf);
void http_mkv_video(http_worker_t *worker, FILE *stream);
void http_mp4_video(http_worker_t *worker, FILE *stream);
void http_mov_video(http_worker_t *worker, FILE *stream);

#define HTTP_LOW_RES_PARAM "res=low"
