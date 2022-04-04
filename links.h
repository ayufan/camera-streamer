#pragma once

#include "v4l2.h"

typedef void (*link_on_buffer)(struct buffer_s *buf);
typedef bool (*link_check_streaming)();

typedef struct link_s {
  struct device_s *capture; // capture_list
  struct device_s *outputs[10];
  struct {
    link_on_buffer on_buffer;
    link_check_streaming check_streaming;
  } callbacks;
} link_t;

int links_init(link_t *all_links);
int links_step(link_t *all_links, int timeout);
int links_loop(link_t *all_links, bool *running);
