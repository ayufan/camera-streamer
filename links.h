#pragma once

#include "v4l2.h"

typedef struct link_s {
  struct device_s *capture; // capture_list
  struct device_s *outputs[10];
  void (*on_buffer)(struct buffer_s *buf);
} link_t;

int handle_links(link_t *all_links, int timeout);
