#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#include "http/http.h"
#include "opts/log.h"

#define BUFSIZE 256

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

static void http_process(http_worker_t *worker, FILE *stream)
{
  // Read headers
  if (!fgets(worker->client_method, sizeof(worker->client_method), stream)) {
    return;
  }

  // Consume headers
  for(int i = 0; i < 50; i++) {
    char line[BUFSIZE];
    if (!fgets(line, BUFSIZE, stream))
      return;
    if (line[0] == '\r' && line[1] == '\n')
      break;
  }

  worker->current_method = NULL;

  for (int i = 0; worker->methods[i].name; i++) {
    const char *name = worker->methods[i].name;
    int nlen = strlen(worker->methods[i].name);

    if (strncmp(worker->client_method, name, nlen-1))
      continue;

    // allow last character to match `?` or ` `
    if (worker->client_method[nlen-1] == name[nlen-1] || name[nlen-1] == '?' && worker->client_method[nlen-1] == ' ') {
      worker->current_method = &worker->methods[i];
      break;
    }
  }

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
  E_LOG_INFO(worker, "Client connected %s.", worker->client_host);

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
    http_process(worker, stream);
    fclose(stream);
  }

  close(worker->client_fd);
  worker->client_fd = -1;

  E_LOG_INFO(worker, "Client disconnected %s.", worker->client_host);
  worker->client_host = NULL;
}

static int http_worker(http_worker_t *worker)
{
  while (1) {
    int addrlen = sizeof(worker->client_addr);
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

  sigaction(SIGPIPE, &(struct sigaction){ SIG_IGN }, NULL);

  for (int worker = 0; worker < options->maxcons; worker++) {
    char name[20];
    sprintf(name, "HTTP%d/%d", options->port, worker);

    http_worker_t *worker = calloc(1, sizeof(http_worker_t));
    worker->name = strdup(name);
    worker->listen_fd = listen_fd;
    worker->methods = methods;
    worker->client_fd = -1;
    pthread_create(&worker->thread, NULL, (void *(*)(void*))http_worker, worker);
  }

  return listen_fd;
}

