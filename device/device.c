#include "device/device.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "util/opts/log.h"
#include "util/opts/opts.h"

device_t *device_open(const char *name, const char *path, device_hw_t *hw) {
  device_t *dev = calloc(1, sizeof(device_t));
  dev->name = strdup(name);
  dev->path = strdup(path);
  dev->hw = hw;
  dev->opts.allow_dma = true;

  if (dev->hw->device_open(dev) < 0) {
		LOG_ERROR(dev, "Can't open device: %s", path);
  }

  return dev;

error:
  device_close(dev);
  return NULL;
}

void device_close(device_t *dev) {
  if(dev == NULL) {
    return;
  }

  if (dev->capture_lists) {
    for (int i = 0; i < dev->n_capture_list; i++) {
      buffer_list_close(dev->capture_lists[i]);
      dev->capture_lists[i] = NULL;
    }
    free(dev->capture_lists);
    dev->capture_lists = NULL;
  }

  if (dev->output_list) {
    buffer_list_close(dev->output_list);
    dev->output_list = NULL;
  }

  dev->hw->device_close(dev);
  free(dev->name);
  free(dev->path);
  free(dev);
}

buffer_list_t *device_open_buffer_list(device_t *dev, bool do_capture, buffer_format_t fmt, bool do_mmap)
{
  return device_open_buffer_list2(dev, NULL, do_capture, fmt, do_mmap);
}

buffer_list_t *device_open_buffer_list2(device_t *dev, const char *path, bool do_capture, buffer_format_t fmt, bool do_mmap)
{
  char name[64];
  int index = 0;
  buffer_list_t *buf_list = NULL;

  if (!dev) {
    return NULL;
  }

  if (!dev->opts.allow_dma) {
    do_mmap = true;
  }

  if (do_capture) {
    index = dev->n_capture_list;

    if (index > 0)
      sprintf(name, "%s:capture:%d", dev->name, index);
    else
      sprintf(name, "%s:capture", dev->name);
  } else {
    if (dev->output_list) {
      LOG_ERROR(dev, "The output_list is already created.");
    }

    sprintf(name, "%s:output", dev->name);
  }

  buf_list = buffer_list_open(name, index, dev, path, fmt, do_capture, do_mmap);
  if (!buf_list) {
    goto error;
  }

  if (do_capture) {
    dev->capture_lists = reallocarray(dev->capture_lists, dev->n_capture_list+1, sizeof(buffer_list_t*));
    dev->capture_lists[dev->n_capture_list++] = buf_list;
  } else {
    dev->output_list = buf_list;
  }

  return buf_list;

error:
  buffer_list_close(buf_list);
  return NULL;
}

buffer_list_t *device_open_buffer_list_output(device_t *dev, buffer_list_t *capture_list)
{
  if (!dev || !capture_list) {
    return NULL;
  }

  buffer_format_t fmt = capture_list->fmt;

  fmt.interval_us = 0;

  bool do_mmap = capture_list->dev->opts.allow_dma ? !capture_list->do_mmap : true;

  // If manually allocating buffers, ensure that `sizeimage` is at least `buf->length`
  if (do_mmap) {
    for (int i = 0; i < capture_list->nbufs; i++) {
      buffer_t *buf = capture_list->bufs[i];
      if (fmt.sizeimage < buf->length)
        fmt.sizeimage = buf->length;
    }
  } else {
    fmt.sizeimage = 0;
  }

  return device_open_buffer_list(dev, false,
    fmt,
    do_mmap);
}

buffer_list_t *device_open_buffer_list_capture(device_t *dev, const char *path, buffer_list_t *output_list, buffer_format_t fmt, bool do_mmap)
{
  if (!dev || !output_list) {
    return NULL;
  }

  if (!fmt.width)
    fmt.width = output_list->fmt.width;
  if (!fmt.height)
    fmt.height = output_list->fmt.height;
  if (!fmt.nbufs)
    fmt.nbufs = output_list->fmt.nbufs;

  return device_open_buffer_list2(dev, path, true, fmt, do_mmap);
}

buffer_list_t *device_open_buffer_list_capture2(device_t *dev, const char *path, buffer_list_t *output_list, unsigned choosen_format, bool do_mmap)
{
  buffer_format_t fmt = {
    .format = choosen_format
  };

  return device_open_buffer_list_capture(dev, path, output_list, fmt, do_mmap);
}

int device_set_stream(device_t *dev, bool do_on)
{
  // TODO: support events
#if 0
  struct v4l2_event_subscription sub = {0};
  sub.type = V4L2_EVENT_SOURCE_CHANGE;

  LOG_DEBUG(dev, "Subscribing to DV-timings events ...");
  ioctl_retried(dev_name(dev), dev->v4l2->dev_fd, do_on ? VIDIOC_SUBSCRIBE_EVENT : VIDIOC_UNSUBSCRIBE_EVENT, &sub);
#endif

  for (int i = 0; i < dev->n_capture_list; i++) {
    if (buffer_list_set_stream(dev->capture_lists[i], do_on) < 0) {
      return -1;
    }
  }

  if (dev->output_list) {
    if (buffer_list_set_stream(dev->output_list, do_on) < 0) {
      return -1;
    }
  }

  return 0;
}

int device_video_force_key(device_t *dev)
{
  if (dev && dev->hw->device_video_force_key)
    return dev->hw->device_video_force_key(dev);

  return -1;
}

void device_dump_options(device_t *dev, FILE *stream)
{
  if (dev && dev->hw->device_dump_options) {
    dev->hw->device_dump_options(dev, stream);
  }
}

int device_dump_options2(device_t *dev, device_option_fn fn, void *opaque)
{
  if (dev && dev->hw->device_dump_options) {
    return dev->hw->device_dump_options2(dev, fn, opaque);
  }
  return -1;
}

int device_set_fps(device_t *dev, int desired_fps)
{
  if (!dev)
    return -1;

  unsigned interval_us = 0;

  if (desired_fps > 0) {
    interval_us = 1000 * 1000 / desired_fps;
  }

  // try to use HW fps setting
  if (dev->hw->device_set_fps && dev->hw->device_set_fps(dev, desired_fps) >= 0) {
    interval_us = 0;
  }

  LOG_INFO(dev, "Setting frame interval_us=%d for FPS=%d", interval_us, desired_fps);

  for (int i = 0; i < dev->n_capture_list; i++) {
    dev->capture_lists[i]->fmt.interval_us = interval_us;
  }

  return 0;
}

int device_set_rotation(device_t *dev, bool vflip, bool hflip)
{
  if (!dev)
    return -1;

  if (dev->hw->device_set_rotation) {
    return dev->hw->device_set_rotation(dev, vflip, hflip);
  }

  int hret = device_set_option_string(dev, "horizontal_flip", hflip ? "1" : "0");
  int vret = device_set_option_string(dev, "vertical_flip", vflip ? "1" : "0");
  return hret ? hret : vret;
}

int device_set_option_string(device_t *dev, const char *key, const char *value)
{
  if (dev && dev->hw->device_set_option) {
    return dev->hw->device_set_option(dev, key, value);
  }

  return -1;
}

int device_set_option_list(device_t *dev, const char *option_list)
{
  if (!dev || !option_list || !option_list[0]) {
    return 0;
  }

  char *start = strdup(option_list);
  char *string = start;
  char *option;
  bool err = false;
  int count = 0;

  while ((option = strsep(&string, OPTION_VALUE_LIST_SEP)) != NULL) {
    char *value = option;
    char *key = strsep(&value, "=");

    if (value) {
      if (device_set_option_string(dev, key, value) < 0) {
        err = true;
      }
    } else {
      LOG_INFO(dev, "Missing 'key=value' for '%s'", option);
      continue;
    }

    // consume all separators
    while (strsep(&value, "="));

    count++;
  }

  free(start);

  return err ? -count : count;
}

int device_output_enqueued(device_t *dev)
{
  if (dev->output_list)
    return buffer_list_count_enqueued(dev->output_list);
  return 0;
}

int device_capture_enqueued(device_t *dev, int *max)
{
  int min_val = 100;
  int max_val = 0;

  for (int i = 0; i < dev->n_capture_list; i++) {
    int count = buffer_list_count_enqueued(dev->capture_lists[i]);
    min_val = MIN(min_val, count);
    max_val = MAX(max_val, count);
  }

  min_val = MIN(min_val, max_val);
  if (max) {
    *max = max_val;
  }
  return min_val;
}
