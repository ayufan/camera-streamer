#include <stdio.h>
#include <stdlib.h>

#include "http.h"

static void http_write_response(
  FILE *stream,
  const char *status,
  const char *content_type,
  const char *body,
  unsigned content_length)
{
  if (content_length == 0 && body)
    content_length = strlen(body);

  fprintf(stream, "HTTP/1.1 %s\r\n", status ? status : "200 OK");
  fprintf(stream, "Content-Type: %s\r\n", content_type ? content_type : "text/plain");
  if (content_length > 0)
    fprintf(stream, "Content-Length: %d\r\n", content_length);
  fprintf(stream, "\r\n");
  if (body) {
    fwrite(body, 1, content_length, stream);
  }
}

void http_content(http_worker_t *worker, FILE *stream)
{
  if (worker->current_method) {
    if (worker->current_method->content_lengthp) {
      worker->current_method->content_length = *worker->current_method->content_lengthp;
    }

    http_write_response(stream,
      NULL,
      worker->current_method->content_type,
      worker->current_method->content_body,
      worker->current_method->content_length
    );
  } else {
    http_write_response(stream, NULL, NULL, "No data", 0);
  }
}

void http_200(FILE *stream, const char *data)
{
  http_write_response(stream, "200 OK", NULL, data ? data : "Nothing here.\n", 0);
}

void http_404(FILE *stream, const char *data)
{
  http_write_response(stream, "404 Not Found", NULL, data ? data : "Nothing here.\n", 0);
}

void http_500(FILE *stream, const char *data)
{
  http_write_response(stream, "500 Server Error", NULL, data ? data : "Server Error\n", 0);
}
