#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/hw/v4l2.h"

buffer_list_t *buffer_list_open(const char *name, struct device_s *dev, bool do_capture, bool do_mmap)
{
  buffer_list_t *buf_list = calloc(1, sizeof(buffer_list_t));

  buf_list->device = dev;
  buf_list->name = strdup(name);
  buf_list->do_capture = do_capture;
  buf_list->do_mmap = do_mmap;
  return buf_list;

error:
  buffer_list_close(buf_list);
  return NULL;
}

void buffer_list_close(buffer_list_t *buf_list)
{
  if (!buf_list) {
    return;
  }

  if (buf_list->bufs) {
    for (int i = 0; i < buf_list->nbufs; i++) {
      buffer_close(buf_list->bufs[i]);
    }
    free(buf_list->bufs);
    buf_list->bufs = NULL;
    buf_list->nbufs = 0;
  }

  free(buf_list->name);
  free(buf_list);
}

int buffer_list_set_format(buffer_list_t *buf_list, unsigned width, unsigned height, unsigned format, unsigned bytesperline)
{
  return buf_list->device->hw->buffer_list_set_format(buf_list, width, height, format, bytesperline);
}

int buffer_list_set_buffers(buffer_list_t *buf_list, int nbufs)
{
  int got_bufs = buf_list->device->hw->buffer_list_set_buffers(buf_list, nbufs);
  if (got_bufs <= 0) {
    goto error;
  }

  buf_list->bufs = calloc(got_bufs, sizeof(buffer_t*));
  buf_list->nbufs = got_bufs;

  for (unsigned i = 0; i < buf_list->nbufs; i++) {
    char name[64];
    sprintf(name, "%s:buf%d", buf_list->name, i);
    buffer_t *buf = buffer_open(name, buf_list, i);
    if (!buf) {
		  E_LOG_ERROR(buf_list, "Cannot open buffer: %u", i);
      goto error;
    }
    buf_list->bufs[i] = buf;
  }

	E_LOG_DEBUG(buf_list, "Opened %u buffers", buf_list->nbufs);
  return 0;

error:
  return -1;
}

int buffer_list_set_stream(buffer_list_t *buf_list, bool do_on)
{
  if (!buf_list) {
    return -1;
  }

  if (buf_list->streaming == do_on) {
    return 0;
  }

  if (buf_list->device->hw->buffer_list_set_stream(buf_list, do_on) < 0) {
    goto error;
  }
  buf_list->streaming = do_on;

  int enqueued = buffer_list_count_enqueued(buf_list);
  E_LOG_DEBUG(buf_list, "Streaming %s... Was %d of %d enqueud", do_on ? "started" : "stopped", enqueued, buf_list->nbufs);
  return 0;

error:
  return -1;
}
