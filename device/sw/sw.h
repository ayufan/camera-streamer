#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

typedef struct buffer_s buffer_t;
typedef struct buffer_list_s buffer_list_t;
typedef struct device_s device_t;
struct pollfd;

typedef struct device_sw_s {
} device_sw_t;

typedef struct buffer_list_sw_s {
  int send_fds[2], recv_fds[2];
  void *data;
  size_t length;
  pthread_t thread;
} buffer_list_sw_t;

typedef struct buffer_sw_s {
} buffer_sw_t;

int sw_device_open(device_t *dev);
void sw_device_close(device_t *dev);

int sw_buffer_open(buffer_t *buf);
void sw_buffer_close(buffer_t *buf);
int sw_buffer_enqueue(buffer_t *buf, const char *who);
int sw_buffer_list_dequeue(buffer_list_t *buf_list, buffer_t **bufp);
int sw_buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue);

int sw_buffer_list_open(buffer_list_t *buf_list);
void sw_buffer_list_close(buffer_list_t *buf_list);
int sw_buffer_list_set_stream(buffer_list_t *buf_list, bool do_on);

bool sw_device_process_jpeg_capture_buf(buffer_t *output_buf, buffer_t *capture_buf);
void *sw_device_thread(void *output_list);
