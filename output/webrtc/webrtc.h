#pragma once

#include "util/http/http.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"
#include "util/opts/control.h"
#include "device/buffer.h"

// WebRTC
void http_webrtc_offer(http_worker_t *worker, FILE *stream);
void webrtc_server();
