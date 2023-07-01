#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <netinet/ip.h>

typedef struct buffer_s buffer_t;
typedef struct http_worker_s http_worker_t;

typedef void (*http_method_fn)(struct http_worker_s *worker, FILE *stream);
typedef void *(*http_param_fn)(struct http_worker_s *worker, FILE *stream, const char *key, const char *value, void *opaque);

#define BUFSIZE 256

typedef struct http_method_s {
  const char *method;
  const char *uri;
  http_method_fn func;
  const char *content_type;
  const void *content_body;
  unsigned content_length;
  unsigned *content_lengthp;
} http_method_t;

typedef struct http_server_options_s {
  char listen[512];
  unsigned port;
  unsigned maxcons;
} http_server_options_t;

typedef struct http_worker_s {
  char *name;
  int listen_fd;
  http_method_t *methods;
  pthread_t thread;
  http_server_options_t options;

  int client_fd;
  int content_length;
  struct sockaddr_in client_addr;
  char *client_host;
  char client_method[BUFSIZE];
  char range_header[BUFSIZE];
  char user_agent[BUFSIZE];
  char host[BUFSIZE];
  char *request_method;
  char *request_uri;
  char *request_params;
  char *request_version;

  http_method_t *current_method;
} http_worker_t;

int http_server(http_server_options_t *options, http_method_t *methods);
void http_content(http_worker_t *worker, FILE *stream);
void http_write_response(FILE *stream, const char *status, const char *content_type, const char *body, unsigned content_length);
void http_write_responsef(FILE *stream, const char *status, const char *content_type, const char *fmt, ...);
void http_200(FILE *stream, const char *data);
void http_400(FILE *stream, const char *data);
void http_404(FILE *stream, const char *data);
void http_500(FILE *stream, const char *data);
void *http_enum_params(http_worker_t *worker, FILE *stream, http_param_fn fn, void *opaque);
char *http_get_param(http_worker_t *worker, const char *key);
