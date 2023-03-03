#pragma once

typedef struct rtsp_options_s {
  bool running;
  uint port;
  int clients;
  int frames;
  int truncated;
  int dropped;
} rtsp_options_t;

int rtsp_server(rtsp_options_t *options);
