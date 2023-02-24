#pragma once

#include <stdint.h>
#include <stdbool.h>

#define LINKS_LOOP_INTERVAL 100
#define MAX_OUTPUT_LISTS 10
#define MAX_CALLBACKS 10

typedef struct buffer_s buffer_t;
typedef struct buffer_list_s buffer_list_t;
typedef struct buffer_lock_s buffer_lock_t;
typedef struct link_s link_t;

typedef void (*link_on_buffer)(buffer_t *buf);
typedef bool (*link_check_streaming)();

typedef struct link_callbacks_s {
  const char *name;
  link_on_buffer on_buffer;
  link_check_streaming check_streaming;
  buffer_lock_t *buf_lock;
} link_callbacks_t;

typedef struct link_s {
  buffer_list_t *capture_list;
  buffer_list_t *output_lists[MAX_OUTPUT_LISTS];
  int n_output_lists;
  link_callbacks_t callbacks[MAX_CALLBACKS];
  int n_callbacks;
} link_t;

int links_loop(link_t *all_links, bool force_active, bool *running);
void links_dump(link_t *all_links);
