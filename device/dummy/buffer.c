#include "dummy.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "util/opts/log.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

int dummy_buffer_open(buffer_t *buf)
{
  buf->dummy = calloc(1, sizeof(buffer_dummy_t));
  buf->start = buf->buf_list->dummy->data;
  buf->used = buf->buf_list->dummy->length;
  buf->length = buf->buf_list->dummy->length;
  return 0;
}

void dummy_buffer_close(buffer_t *buf)
{
  free(buf->dummy);
}

int dummy_buffer_enqueue(buffer_t *buf, const char *who)
{
  unsigned index = buf->index;
  if (write(buf->buf_list->dummy->fds[1], &index, sizeof(index)) != sizeof(index)) {
    return -1;
  }
  return 0;
}

int dummy_buffer_list_dequeue(buffer_list_t *buf_list, buffer_t **bufp)
{
  unsigned index = 0;
  int n = read(buf_list->dummy->fds[0], &index, sizeof(index));
  if (n != sizeof(index)) {
    LOG_INFO(buf_list, "Received invalid result from `read`: %d", n);
    return -1;
  }

  if (index >= (unsigned)buf_list->nbufs) {
    LOG_INFO(buf_list, "Received invalid index from `read`: %d >= %d", index, buf_list->nbufs);
    return -1;
  }

  *bufp = buf_list->bufs[index];
  return 0;
}

int dummy_buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue)
{
  int count_enqueued = buffer_list_count_enqueued(buf_list);
  pollfd->fd = buf_list->dummy->fds[0]; // write end
  pollfd->events = POLLHUP;
  if (can_dequeue && count_enqueued > 0) {
    pollfd->events |= POLLIN;
  }
  pollfd->revents = 0;
  return 0;
}
