#pragma once

#include "http/http.h"
#include "device/buffer.h"
#include "opts/log.h"
#include "opts/fourcc.h"
#include "opts/control.h"

// WebRTC
bool http_webrtc_needs_buffer();
void http_webrtc_capture(buffer_t *buf);
void http_webrtc_low_res_capture(buffer_t *buf);
void http_webrtc_offer(http_worker_t *worker, FILE *stream);
