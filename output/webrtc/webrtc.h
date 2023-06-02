#pragma once

#include <stdio.h>

#define WEBRTC_OPTIONS_LENGTH 4096

typedef struct http_worker_s http_worker_t;

typedef struct webrtc_options_s {
  bool running;
  bool disabled;
  char ice_servers[WEBRTC_OPTIONS_LENGTH];
} webrtc_options_t;

// WebRTC
void http_webrtc_offer(http_worker_t *worker, FILE *stream);
int webrtc_server(webrtc_options_t *options);
