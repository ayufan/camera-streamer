#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#include "http.h"
#include "util/opts/log.h"

#define HEADER_RANGE "Range:"
#define HEADER_CONTENT_LENGTH "Content-Length:"
#define HEADER_USER_AGENT "User-Agent:"
#define HEADER_HOST "Host:"

static int http_listen(int port, int maxcons)
{
  struct sockaddr_in server = {0};
  int listenfd = -1;

  // getaddrinfo for host
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(port);

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    perror("socket");
    return -1;
  }

  int optval = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

  if (bind(listenfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("bind");
    goto error;
  }

  if (listen(listenfd, maxcons) < 0) {
    perror("listen");
    goto error;
  }

  return listenfd;
error:
  if (listenfd >= 0)
    close(listenfd);
  return -1;
}

void *http_enum_params(http_worker_t *worker, FILE *stream, http_param_fn fn, void *opaque)
{
  const char *params = worker->request_params;
  if (!params || !params[0]) {
    return NULL;
  }

  void *ret = NULL;
  char *start = strdup(params);
  char *string = start;
  char *option;

  while ((option = strsep(&string, "&")) != NULL) {
    char *value = option;
    char *key = strsep(&value, "=");

    ret = fn(worker, stream, key, value, opaque);
    if (ret) {
      break;
    }

    // consume all separators
    while (strsep(&value, "="));
  }

  free(start);
  return ret;
}

static void *http_get_param_fn(struct http_worker_s *worker, FILE *stream, const char *key, const char *value, void *opaque)
{
  if (!strcmp(key, opaque))
    return (void*)value;

  return NULL;
}

const char *http_get_param(http_worker_t *worker, const char *key)
{
  return http_enum_params(worker, NULL, http_get_param_fn, (void*)key);
}

static void http_process(http_worker_t *worker, FILE *stream)
{
  // Read headers
  if (!fgets(worker->client_method, sizeof(worker->client_method), stream)) {
    return;
  }

  worker->range_header[0] = 0;
  worker->user_agent[0] = 0;
  worker->host[0] = 0;
  worker->content_length = -1;

  // request_uri
  worker->request_method = worker->client_method;

  if ((worker->request_uri = strstr(worker->request_method, " ")) != NULL) {
    *worker->request_uri++ = 0;
  } else {
    worker->request_uri = "";
  }

  if ((worker->request_version = strstr(worker->request_uri, " ")) != NULL) {
    *worker->request_version++ = 0;
  } else {
    worker->request_version = "";
  }

  if ((worker->request_params = strstr(worker->request_uri, "?")) != NULL) {
    *worker->request_params++ = 0;
  } else {
    worker->request_params = "";
  }

  // Consume headers
  for(int i = 0; i < 50; i++) {
    char line[BUFSIZE];
    if (!fgets(line, BUFSIZE, stream))
      return;
    if (line[0] == '\r' && line[1] == '\n')
      break;

    if (strcasestr(line, HEADER_RANGE) == line) {
      strcpy(worker->range_header, trim(line));
    } else if (strcasestr(line, HEADER_CONTENT_LENGTH) == line) {
      worker->content_length = atoi(trim(line + strlen(HEADER_CONTENT_LENGTH)));
    } else if (strcasestr(line, HEADER_USER_AGENT) == line) {
      strcpy(worker->user_agent, trim(line + strlen(HEADER_USER_AGENT)));
    } else if (strcasestr(line, HEADER_HOST) == line) {
      strcpy(worker->host, trim(line + strlen(HEADER_HOST)));
    }
  }

  worker->current_method = NULL;

  for (int i = 0; worker->methods[i].method; i++) {
    http_method_t *method = &worker->methods[i];

    if (strcmp(worker->request_method, method->method))
      continue;

    const char *params = strstr(method->uri, "?");

    if (params) {
      // match request_uri and params
      if (strncmp(worker->request_uri, method->uri, params - method->uri))
        continue;
      if (!strstr(worker->request_params, params+1))
        continue;
    } else if (method->uri[0] == '*') {
      if (strstr(worker->request_uri, method->uri + 1) != worker->request_uri)
        continue;
    } else {
      if (strcmp(worker->request_uri, method->uri))
        continue;
    }

    worker->current_method = &worker->methods[i];
    break;
  }

  LOG_INFO(worker, "Request '%s' '%s' '%s'", worker->request_method, worker->request_uri, worker->request_params);

  if (worker->current_method) {
    worker->current_method->func(worker, stream);
    worker->current_method = NULL;
    return;
  }

  http_404(stream, "Not found.");
}

static void http_client(http_worker_t *worker)
{
  worker->client_host = inet_ntoa(worker->client_addr.sin_addr);
  LOG_INFO(worker, "Client connected %s (fd=%d).", worker->client_host, worker->client_fd);

  struct timeval tv;
  tv.tv_sec = 3;
  tv.tv_usec = 0;
  setsockopt(worker->client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
  setsockopt(worker->client_fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

  int on = 1;
  setsockopt(worker->client_fd, IPPROTO_TCP, TCP_NODELAY, (void *)&on, sizeof(on));
  setsockopt(worker->client_fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&on, sizeof(on));

  FILE *stream = fdopen(worker->client_fd, "r+");
  if (stream) {
    worker->client_fd = -1; // ownership taken by stream

    http_process(worker, stream);
    fclose(stream);
  }

  if (worker->client_fd >= 0) {
    close(worker->client_fd);
    worker->client_fd = -1;
  }

  LOG_INFO(worker, "Client disconnected %s.", worker->client_host);
  worker->client_host = NULL;
}

static int http_worker(http_worker_t *worker)
{
  while (1) {
    unsigned addrlen = sizeof(worker->client_addr);
    worker->client_fd = accept(worker->listen_fd, (struct sockaddr *)&worker->client_addr, &addrlen);
    if (worker->client_fd < 0) {
      goto error;
    }

    http_client(worker);
  }

error:
  free(worker->name);
  free(worker);
  return -1;
}

int http_server(http_server_options_t *options, http_method_t *methods)
{
  int listen_fd = http_listen(options->port, options->maxcons);
  if (listen_fd < 0) {
    return -1;
  }

  sigaction(SIGPIPE, &(struct sigaction){{ SIG_IGN }}, NULL);

  for (int worker = 0; worker < options->maxcons; worker++) {
    char name[20];
    sprintf(name, "HTTP%d/%d", options->port, worker);

    http_worker_t *worker = calloc(1, sizeof(http_worker_t));
    worker->name = strdup(name);
    worker->listen_fd = listen_fd;
    worker->methods = methods;
    worker->client_fd = -1;
    worker->options = *options;
    pthread_create(&worker->thread, NULL, (void *(*)(void*))http_worker, worker);
  }

  return listen_fd;
}

