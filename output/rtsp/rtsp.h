#pragma once

typedef struct rtsp_options_s {
  uint port;
} rtsp_options_t;

extern rtsp_options_t rtsp_options;

int rtsp_server();
bool rtsp_h264_needs_buffer();
void rtsp_h264_capture(struct buffer_s *buf);
void rtsp_h264_low_res_capture(struct buffer_s *buf);
