#include "sw.h"

#include "device/device.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"

static buffer_t *sw_device_recv_send_buffer(buffer_list_t *buf_list)
{
  unsigned index = 0;
  int n = read(buf_list->sw->send_fds[0], &index, sizeof(index));
  if (n != sizeof(index)) {
    LOG_INFO(buf_list, "Received invalid result from `read`: %d", n);
    return NULL;
  }

  if (index >= (unsigned)buf_list->nbufs) {
    LOG_INFO(buf_list, "Received invalid index from `read`: %d >= %d", index, buf_list->nbufs);
    return NULL;
  }

  return buf_list->bufs[index];
}

static int sw_device_send_recv_buffer(buffer_t *buf)
{
  unsigned index = buf->index;
  if (write(buf->buf_list->sw->recv_fds[1], &index, sizeof(index)) != sizeof(index)) {
    return -1;
  }
  return 0;
}

static void sw_device_process_capture_buf(buffer_t *output_buf, buffer_t *capture_buf)
{
  capture_buf->length = 0;
}

void *sw_device_thread(buffer_list_t *output_list)
{
  while (output_list->streaming) {
    buffer_t *output_buf = sw_device_recv_send_buffer(output_list);
    if (!output_buf) {
      continue;
    }

    buffer_t *capture_buf = sw_device_recv_send_buffer(
      output_list->dev->capture_lists[0]);

    if (capture_buf) {
      sw_device_process_capture_buf(output_buf, capture_buf);
      sw_device_send_recv_buffer(capture_buf);
    }

    sw_device_send_recv_buffer(output_buf);
  }

  return NULL;
}
