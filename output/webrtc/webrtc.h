#pragma once

#include "util/http/http.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"
#include "util/opts/control.h"
#include "device/buffer.h"

// WebRTC
bool http_webrtc_needs_buffer();
void http_webrtc_capture(buffer_t *buf);
void http_webrtc_low_res_capture(buffer_t *buf);
void http_webrtc_offer(http_worker_t *worker, FILE *stream);
