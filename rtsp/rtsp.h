#pragma once

int rtsp_server();
bool rtsp_h264_needs_buffer();
void rtsp_h264_capture(struct buffer_s *buf);
