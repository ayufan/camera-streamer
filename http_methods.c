#include <stdio.h>
#include <stdlib.h>

#include "http.h"
#include "buffer.h"
#include "buffer_lock.h"

void http_index(http_worker_t *worker, FILE *stream)
{
  fprintf(stream, "HTTP/1.1 200 OK\r\n");
  fprintf(stream, "Content-Type: text/plain\r\n");
  fprintf(stream, "\r\n");
  fprintf(stream, "Text.\r\n");
  fflush(stream);
}

void http_404_header(http_worker_t *worker, FILE *stream)
{
  fprintf(stream, "HTTP/1.1 404 Not Found\r\n");
  fprintf(stream, "Content-Type: text/plain\r\n");
  fprintf(stream, "\r\n");
}

void http_404(http_worker_t *worker, FILE *stream)
{
  http_404_header(worker, stream);
  fprintf(stream, "Nothing here?\r\n");
}
