#include <stdio.h>
#include <stdlib.h>

#include "http/http.h"
#include "hw/buffer.h"
#include "hw/buffer_lock.h"

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

void http_video_html(http_worker_t *worker, FILE *stream)
{
  extern unsigned char html_video_html[];
  extern unsigned int html_video_html_len;

  fprintf(stream, "HTTP/1.1 200 OK\r\n");
  fprintf(stream, "Content-Type: text/html;charset=UTF-8\r\n");
  fprintf(stream, "\r\n");
  fwrite(html_video_html, 1, html_video_html_len, stream);
  fflush(stream);
}

void http_jmuxer_js(http_worker_t *worker, FILE *stream)
{
  extern unsigned char html_jmuxer_min_js[];
  extern unsigned int html_jmuxer_min_js_len;

  fprintf(stream, "HTTP/1.1 200 OK\r\n");
  fprintf(stream, "Content-Type: text/javascript;charset=UTF-8\r\n");
  fprintf(stream, "\r\n");
  fwrite(html_jmuxer_min_js, 1, html_jmuxer_min_js_len, stream);
  fflush(stream);
}
