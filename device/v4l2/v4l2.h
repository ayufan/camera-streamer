#pragma once

// TODO: temporary
#include "device/hw/v4l2.h"

typedef struct buffer_s buffer_t;
typedef struct buffer_list_s buffer_list_t;
struct pollfd;

int v4l2_buffer_open(buffer_t *buf);
void v4l2_buffer_close(buffer_t *buf);
int v4l2_buffer_enqueue(buffer_t *buf, const char *who);
int v4l2_buffer_list_dequeue(buffer_list_t *buf_list, buffer_t **bufp);
int v4l2_buffer_list_refresh_states(buffer_list_t *buf_list);
int v4l2_buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue);
