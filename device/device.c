#include "device/device.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "opts/log.h"
#include "opts/opts.h"

device_t *device_open(const char *name, const char *path, device_hw_t *hw) {
  device_t *dev = calloc(1, sizeof(device_t));
  dev->name = strdup(name);
  dev->path = strdup(path);
  dev->hw = hw;
  dev->allow_dma = true;

  if (dev->hw->device_open(dev) < 0) {
		E_LOG_ERROR(dev, "Can't open device: %s", path);
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

  if (dev->capture_list) {
    buffer_list_close(dev->capture_list);
    dev->capture_list = NULL;
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

int device_open_buffer_list(device_t *dev, bool do_capture, unsigned width, unsigned height, unsigned format, unsigned bytesperline, int nbufs, bool do_mmap)
{
  unsigned type;
  char name[64];
  struct buffer_list_s **buf_list = NULL;

  if (!dev) {
    return -1;
  }

  if (!dev->allow_dma) {
    do_mmap = true;
  }

  if (do_capture) {
    buf_list = &dev->capture_list;

    if (dev->capture_list) {
      E_LOG_ERROR(dev, "The capture_list is already created.");
    }

    sprintf(name, "%s:capture", dev->name);
  } else {
    buf_list = &dev->output_list;

    if (dev->output_list) {
      E_LOG_ERROR(dev, "The output_list is already created.");
    }

    sprintf(name, "%s:output", dev->name);
  }

  *buf_list = buffer_list_open(name, dev, do_capture, do_mmap);
  if (!*buf_list) {
    goto error;
  }

  if (buffer_list_set_format(*buf_list, width, height, format, bytesperline) < 0) {
    goto error;
  }

  if (buffer_list_set_buffers(*buf_list, nbufs) < 0) {
    goto error;
  }

  return 0;

error:
  buffer_list_close(*buf_list);
  *buf_list = NULL;
  return -1;
}

int device_open_buffer_list_output(device_t *dev, buffer_list_t *capture_list)
{
  return device_open_buffer_list(dev, false,
    capture_list->fmt_width, capture_list->fmt_height,
    capture_list->fmt_format, capture_list->fmt_bytesperline,
    capture_list->nbufs,
    capture_list->device->allow_dma ? !capture_list->do_mmap : true);
}

int device_open_buffer_list_capture(device_t *dev, buffer_list_t *output_list, float div, unsigned format, bool do_mmap)
{
  if (!output_list) {
    output_list = dev->output_list;
  }
  if (!output_list) {
    return -1;
  }

  return device_open_buffer_list(dev, true,
    output_list->fmt_width / div, output_list->fmt_height / div,
    format, 0, output_list->nbufs, do_mmap);
}

int device_set_stream(device_t *dev, bool do_on)
{
  // TODO: support events
#if 0
  struct v4l2_event_subscription sub = {0};
  sub.type = V4L2_EVENT_SOURCE_CHANGE;

  E_LOG_DEBUG(dev, "Subscribing to DV-timings events ...");
  xioctl(dev_name(dev), dev->v4l2.dev_fd, do_on ? VIDIOC_SUBSCRIBE_EVENT : VIDIOC_UNSUBSCRIBE_EVENT, &sub);
#endif

  if (dev->capture_list) {
    if (buffer_list_set_stream(dev->capture_list, do_on) < 0) {
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

int device_consume_event(device_t *dev)
{
  // TODO: support events

#if 0
	struct v4l2_event event;

  if (!dev) {
    return -1;
  }

	E_LOG_DEBUG(dev, "Consuming V4L2 event ...");
  E_XIOCTL(dev, dev->v4l2.dev_fd, VIDIOC_DQEVENT, &event, "Got some V4L2 device event, but where is it?");

  switch (event.type) {
    case V4L2_EVENT_SOURCE_CHANGE:
      E_LOG_INFO(dev, "Got V4L2_EVENT_SOURCE_CHANGE: source changed");
      return -1;
    case V4L2_EVENT_EOS:
      E_LOG_INFO(dev, "Got V4L2_EVENT_EOS: end of stream (ignored)");
      return 0;
  }

  return 0;

error:
#endif

  return -1;
}

int device_set_decoder_start(device_t *dev, bool do_on)
{
  if (!dev || dev->hw->device_set_decoder_start(dev, do_on) < 0)
    return -1;

  dev->decoder_started = do_on;
  return 0;
}

int device_video_force_key(device_t *dev)
{
  if (!dev || dev->hw->device_video_force_key(dev) < 0)
    return -1;

  return 0;
}

int device_set_fps(device_t *dev, int desired_fps)
{
  if (!dev || dev->hw->device_set_fps(dev, desired_fps) < 0)
    return -1;

  return 0;
}

int device_set_option_string(device_t *dev, const char *key, const char *value)
{
  if (!dev) {
    return -1;
  }

  return dev->hw->device_set_option(dev, key, value);
}

void device_set_option_list(device_t *dev, const char *option_list)
{
  if (!dev || !option_list || !option_list[0]) {
    return;
  }

  char *start = strdup(option_list);
  char *string = start;
  char *option;

  while (option = strsep(&string, OPTION_VALUE_LIST_SEP)) {
    char *value = option;
    char *key = strsep(&value, "=");

    if (value) {
      device_set_option_string(dev, key, value);
    } else {
      E_LOG_INFO(dev, "Missing 'key=value' for '%s'", option);
      continue;
    }

    // consume all separators
    while (strsep(&value, "="));
  }

  free(start);
}
