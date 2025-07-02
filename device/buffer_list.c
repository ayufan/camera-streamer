#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"

static int buffer_list_alloc_buffers2(buffer_list_t *buf_list, int got_bufs)
{
  if (buf_list->bufs || got_bufs <= 0) {
    return -1;
  }

	LOG_INFO(
    buf_list,
    "Using: %ux%u/%s, buffers=%d, bytesperline=%d, sizeimage=%.1fMiB",
    buf_list->fmt.width,
    buf_list->fmt.height,
    fourcc_to_string(buf_list->fmt.format).buf,
    got_bufs,
    buf_list->fmt.bytesperline,
    buf_list->fmt.sizeimage / 1024.0f / 1024.0f
  );

  buf_list->bufs = calloc(got_bufs, sizeof(buffer_t*));
  buf_list->fmt.nbufs = got_bufs;
  buf_list->nbufs = got_bufs;

  unsigned mem_used = 0;

  for (unsigned i = 0; i < buf_list->nbufs; i++) {
    char name[64];
    sprintf(name, "%s:buf%d", buf_list->name, i);
    buffer_t *buf = buffer_open(name, buf_list, i);
    if (!buf) {
      LOG_ERROR(buf_list, "Cannot open buffer: %u", i);
      goto error;
    }

    if (buf->dma_fd >= 0) {
      mem_used += buf->length;
    }

    buf_list->bufs[i] = buf;
  }

  LOG_INFO(buf_list, "Opened %u buffers. Memory used: %.1f MiB", buf_list->nbufs, mem_used / 1024.0f / 1024.0f);
  return 0;

error:
  buffer_list_free_buffers(buf_list);
  return -1;
}

buffer_list_t *buffer_list_open(const char *name, int index, struct device_s *dev, const char *path, buffer_format_t fmt, bool do_capture, bool do_mmap)
{
  buffer_list_t *buf_list = calloc(1, sizeof(buffer_list_t));

  buf_list->dev = dev;
  buf_list->name = strdup(name);
  if (path) {
    buf_list->path = strdup(path);
  }
  buf_list->do_capture = do_capture;
  buf_list->do_mmap = do_mmap;
  buf_list->fmt = fmt;
  buf_list->index = index;

  int err = dev->hw->buffer_list_open(buf_list);
  if (err > 0) {
    err = buffer_list_alloc_buffers2(buf_list, err);
  }

  if (err < 0) {
    goto error;
  }

  return buf_list;

error:
  buffer_list_close(buf_list);
  return NULL;
}

int buffer_list_alloc_buffers(buffer_list_t *buf_list)
{
  if (buf_list->bufs) {
    return 0;
  }
  if (!buf_list->dev->hw->buffer_list_alloc_buffers) {
    return -1;
  }

  int got_bufs = buf_list->dev->hw->buffer_list_alloc_buffers(buf_list);
  if (got_bufs < 0) {
    return -1;
  }

  return buffer_list_alloc_buffers2(buf_list, got_bufs);
}

void buffer_list_free_buffers(buffer_list_t *buf_list)
{
  if (!buf_list->bufs) {
    return;
  }

  for (int i = 0; i < buf_list->nbufs; i++) {
    buffer_close(buf_list->bufs[i]);
  }
  free(buf_list->bufs);
  buf_list->bufs = NULL;
  buf_list->nbufs = 0;

  if (buf_list->dev->hw->buffer_list_free_buffers) {
    buf_list->dev->hw->buffer_list_free_buffers(buf_list);
  }
}

void buffer_list_close(buffer_list_t *buf_list)
{
  if (!buf_list) {
    return;
  }

  buffer_list_free_buffers(buf_list);

  buf_list->dev->hw->buffer_list_close(buf_list);
  free(buf_list->name);
  free(buf_list);
}

int buffer_list_set_stream(buffer_list_t *buf_list, bool do_on)
{
  if (!buf_list || !buf_list->dev->hw->buffer_list_set_stream) {
    return -1;
  }

  if (buf_list->streaming == do_on) {
    return 0;
  }

  buf_list->streaming = do_on;

  if (buf_list->dev->hw->buffer_list_set_stream(buf_list, do_on) < 0) {
    buf_list->streaming = !do_on;
    goto error;
  }

  if (do_on) {
    buf_list->last_enqueued_us = get_monotonic_time_us(NULL, NULL);
  } else {
    buffer_list_clear_queue(buf_list);
  }

  int enqueued = buffer_list_count_enqueued(buf_list);
  LOG_INFO(buf_list, "Streaming %s... Was %d of %d enqueud", do_on ? "started" : "stopped", enqueued, buf_list->nbufs);
  return 0;

error:
  return -1;
}
