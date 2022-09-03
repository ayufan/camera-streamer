#pragma once

typedef struct rtsp_options_s {
  uint port;
} rtsp_options_t;

int rtsp_server(rtsp_options_t *options);
