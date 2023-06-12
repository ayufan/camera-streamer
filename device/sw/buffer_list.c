#include "sw.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

int sw_buffer_list_open(buffer_list_t *buf_list)
{
  buf_list->do_mmap = true;

  if (buf_list->do_capture) {
    switch (buf_list->fmt.format) {
    case V4L2_PIX_FMT_YUYV:
      break;

    default:
      LOG_INFO(buf_list, "The format is not supported '%s'",
        fourcc_to_string(buf_list->fmt.format).buf);
      return -1;
    }
  } else {
    switch (buf_list->fmt.format) {
#ifdef USE_LIBJPEG
    case V4L2_PIX_FMT_JPEG:
    case V4L2_PIX_FMT_MJPEG:
      break;
#endif // USE_LIBJPEG

    default:
      LOG_INFO(buf_list, "The format is not supported '%s'",
        fourcc_to_string(buf_list->fmt.format).buf);
      return -1;
    }
  }

  buf_list->sw = calloc(1, sizeof(buffer_list_sw_t));
  buf_list->sw->send_fds[0] = -1;
  buf_list->sw->send_fds[1] = -1;
  buf_list->sw->recv_fds[0] = -1;
  buf_list->sw->recv_fds[1] = -1;

  if (pipe2(buf_list->sw->send_fds, O_DIRECT|O_CLOEXEC) < 0) {
    LOG_INFO(buf_list, "Cannot open `pipe2`.");
    return -1;
  }

  if (pipe2(buf_list->sw->recv_fds, O_DIRECT|O_CLOEXEC) < 0) {
    LOG_INFO(buf_list, "Cannot open `pipe2`.");
    close(buf_list->sw->send_fds[0]);
    close(buf_list->sw->send_fds[1]);
    return -1;
  }

  return buf_list->fmt.nbufs;
}

void sw_buffer_list_close(buffer_list_t *buf_list)
{
  if (buf_list->sw) {
    close(buf_list->sw->send_fds[0]);
    close(buf_list->sw->send_fds[1]);
    close(buf_list->sw->recv_fds[0]);
    close(buf_list->sw->recv_fds[1]);
    free(buf_list->sw->data);
  }

  free(buf_list->sw);
}

int sw_buffer_list_set_stream(buffer_list_t *buf_list, bool do_on)
{
  if (do_on && !buf_list->do_capture) {
    pthread_create(&buf_list->sw->thread, NULL, sw_device_thread, buf_list);
  }
  return 0;
}
