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

static bool sw_device_process_capture_buf(buffer_t *output_buf, buffer_t *capture_buf)
{
  capture_buf->used = 0;
  capture_buf->flags.is_keyed = true;
  capture_buf->flags.is_keyframe = true;
  capture_buf->flags.is_last = false;
  capture_buf->enqueue_time_us = output_buf->enqueue_time_us;
  capture_buf->captured_time_us = output_buf->captured_time_us;

  switch (output_buf->buf_list->fmt.format) {
  case V4L2_PIX_FMT_JPEG:
  case V4L2_PIX_FMT_MJPEG:
    if (capture_buf->buf_list->fmt.format == V4L2_PIX_FMT_YUYV) {
      return sw_device_process_jpeg_capture_buf(output_buf, capture_buf);
    }
    LOG_INFO(capture_buf, "The format is not supported '%s'",
      fourcc_to_string(capture_buf->buf_list->fmt.format).buf);
    return false;

  default:
    LOG_INFO(output_buf, "The format is not supported '%s'",
      fourcc_to_string(output_buf->buf_list->fmt.format).buf);
    return false;
  }
}

void *sw_device_thread(void *output_list_)
{
  buffer_list_t *output_list = output_list_;
  buffer_t *capture_buf = NULL;

  LOG_INFO(output_list, "Starting software thread...");

  while (output_list->streaming) {
    buffer_t *output_buf = sw_device_recv_send_buffer(output_list);
    if (!output_buf) {
      continue;
    }

    if (!capture_buf) {
      capture_buf = sw_device_recv_send_buffer(
        output_list->dev->capture_lists[0]);
    }

    if (sw_device_process_capture_buf(output_buf, capture_buf)) {
      sw_device_send_recv_buffer(capture_buf);
      capture_buf = NULL;
    }

    sw_device_send_recv_buffer(output_buf);
  }

  if (capture_buf) {
    sw_device_send_recv_buffer(capture_buf);
  }

  LOG_INFO(output_list, "Stopped software thread.");
  return NULL;
}
