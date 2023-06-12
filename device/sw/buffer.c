#include "sw.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

// Taken from https://github.com/raspberrypi/linux/blob/bb63dc31e48948bc2649357758c7a152210109c4/drivers/staging/vc04_services/bcm2835-codec/bcm2835-v4l2-codec.c#L148
#define DEF_COMP_BUF_SIZE_JPEG (4096 << 10)

int sw_buffer_open(buffer_t *buf)
{
  buf->sw = calloc(1, sizeof(buffer_sw_t));

  switch (buf->buf_list->fmt.format) {
  case V4L2_PIX_FMT_MJPEG:
    buf->length = DEF_COMP_BUF_SIZE_JPEG;
    break;

  case V4L2_PIX_FMT_YUYV:
    buf->length = buf->buf_list->fmt.width * buf->buf_list->fmt.height * 2;
    break;

  default:
    LOG_INFO(buf->buf_list, "The format is not supported '%s'",
      fourcc_to_string(buf->buf_list->fmt.format).buf);
    return -1;
  }

  if (buf->buf_list->do_mmap) {
    buf->start = malloc(buf->length);
    if (!buf->start) {
      LOG_INFO(buf, "Failed to allocate %zu bytes for '%s'",
        buf->length,
        fourcc_to_string(buf->buf_list->fmt.format).buf);
      return -1;
    }
  }

  buf->used = 0;
  return 0;
}

void sw_buffer_close(buffer_t *buf)
{
  if (!buf->buf_list->do_mmap) {
    free(buf->start);
  }
  free(buf->sw);
}

int sw_buffer_enqueue(buffer_t *buf, const char *who)
{
  unsigned index = buf->index;
  if (write(buf->buf_list->sw->send_fds[1], &index, sizeof(index)) != sizeof(index)) {
    return -1;
  }
  return 0;
}

int sw_buffer_list_dequeue(buffer_list_t *buf_list, buffer_t **bufp)
{
  unsigned index = 0;
  int n = read(buf_list->sw->recv_fds[0], &index, sizeof(index));
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

int sw_buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue)
{
  int count_enqueued = buffer_list_count_enqueued(buf_list);
  pollfd->fd = buf_list->sw->recv_fds[0]; // write end
  pollfd->events = POLLHUP;
  if (can_dequeue && count_enqueued > 0) {
    pollfd->events |= POLLIN;
  }
  pollfd->revents = 0;
  return 0;
}
