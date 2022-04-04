#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "http.h"

#define BUFSIZE 256

int http_listen(int listen_port, int maxcons)
{
  struct sockaddr_in server = {0};
  int listenfd = -1;

  // getaddrinfo for host
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(listen_port);

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    return -1;
  }

  if (bind(listenfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
    goto error;
  }

  if (listen(listenfd, maxcons) < 0) {
    goto error;
  }

  return listenfd;
error:
  if (listenfd >= 0)
    close(listenfd);
  return -1;
}

void http_process(int fd, struct sockaddr_in *addr)
{
  FILE *stream = fdopen(fd, "r+");
  if (!stream) {
    return;
  }

  char line[BUFSIZE];
  fgets(line, BUFSIZE, stream);

  fprintf(stream, "HTTP/1.1 200 OK\n");
  fprintf(stream, "Content-Type: text/plain\n");
  fprintf(stream, "\r\n");
  fprintf(stream, "Text.\n");
  fflush(stream);
  fclose(stream);
}

int http_worker(int listenfd)
{
  while (1) {
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);
    int ret = accept(listenfd, (struct sockaddr *)&addr, &addrlen);
    if (ret < 0) {
      return -1;
    }

    http_process(ret, &addr);
    close(ret);
  }

  return -1;
}

int http_worker_threads(int listenfd, int nthreads)
{
  while (nthreads-- > 0) {
    pthread_t thread;
    pthread_create(&thread, NULL, (void *(*)(void*))http_worker, (void*)listenfd);
  }

  return 0;
}
