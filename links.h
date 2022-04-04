#pragma once

#include "v4l2.h"

typedef struct link_s {
  struct device_s *capture; // capture_list
  struct device_s *outputs[10];
  void (*on_buffer)(struct buffer_s *buf);

  unsigned capture_format;
} link_t;

int links_init(link_t *all_links);
int links_step(link_t *all_links, int timeout);
int links_loop(link_t *all_links, bool *running);
