#pragma once

#include <stdint.h>
#include <stdbool.h>

#define LINKS_LOOP_INTERVAL 100

typedef struct buffer_s buffer_t;
typedef struct buffer_list_s buffer_list_t;
typedef struct buffer_lock_s buffer_lock_t;
typedef struct link_s link_t;

typedef void (*link_on_buffer)(buffer_t *buf);
typedef bool (*link_check_streaming)();
typedef bool (*link_validate_buffer)(struct link_s *link, buffer_t *buf);

typedef struct link_callbacks_s {
  const char *name;
  link_on_buffer on_buffer;
  link_check_streaming check_streaming;
  link_validate_buffer validate_buffer;
  buffer_lock_t *buf_lock;
} link_callbacks_t;

typedef struct link_s {
  buffer_list_t *source; // capture_list
  buffer_list_t *sinks[10];
  link_callbacks_t callbacks[10];
  int n_callbacks;
} link_t;

int links_loop(link_t *all_links, bool *running);
void links_dump(link_t *all_links);
