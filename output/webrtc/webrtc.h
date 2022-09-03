#pragma once

#include <stdio.h>

typedef struct http_worker_s http_worker_t;

typedef struct webrtc_options_s {
  bool running;
  bool disabled;
} webrtc_options_t;

// WebRTC
void http_webrtc_offer(http_worker_t *worker, FILE *stream);
int webrtc_server(webrtc_options_t *options);
