#include "dummy.h"
#include "device/buffer.h"

#include <stdlib.h>

int dummy_buffer_open(buffer_t *buf)
{
  buf->dummy = calloc(1, sizeof(buffer_dummy_t));
  return 0;
}

void dummy_buffer_close(buffer_t *buf)
{
  free(buf->dummy);
}

int dummy_buffer_enqueue(buffer_t *buf, const char *who)
{
  return -1;
}

int dummy_buffer_list_dequeue(buffer_list_t *buf_list, buffer_t **bufp)
{
  return -1;
}

int dummy_buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue)
{
  return -1;
}
