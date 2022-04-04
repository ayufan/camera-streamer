#pragma once

#include "v4l2.h"

typedef struct link_s {
  device_t *capture; // capture_list
  device_t *outputs[10];
  void (*on_buffer)(buffer_t *buf);
} link_t;

int handle_links(link_t *all_links, int timeout);
