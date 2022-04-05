#include "hw/device.h"
#include "hw/buffer.h"
#include "hw/buffer_list.h"

device_t *device_open(const char *name, const char *path) {
  device_t *dev = calloc(1, sizeof(device_t));
  dev->name = strdup(name);
  dev->path = strdup(path);
  dev->fd = open(path, O_RDWR|O_NONBLOCK);
  dev->allow_dma = true;
  if(dev->fd < 0) {
		E_LOG_ERROR(dev, "Can't open device");
	}

	E_LOG_DEBUG(dev, "Querying device capabilities ...");
  E_XIOCTL(dev, dev->fd, VIDIOC_QUERYCAP, &dev->v4l2_cap, "Can't query device capabilities");\

	if (!(dev->v4l2_cap.capabilities & V4L2_CAP_STREAMING)) {
		E_LOG_ERROR(dev, "Device doesn't support streaming IO");
	}

	E_LOG_INFO(dev, "Device path=%s fd=%d opened", dev->path, dev->fd);
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

  if(dev->fd >= 0) {
    close(dev->fd);
  }

  free(dev);
}

int device_open_buffer_list(device_t *dev, bool do_capture, unsigned width, unsigned height, unsigned format, int nbufs)
{
  unsigned type;
  char name[64];
  struct buffer_list_s **buf_list = NULL;
  bool do_mmap = false;

  if (!dev) {
    return -1;
  }

  if (do_capture) {
    buf_list = &dev->capture_list;
    do_mmap = true;

    if (dev->capture_list) {
      E_LOG_ERROR(dev, "The capture_list is already created.");
    }

    if (dev->v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      sprintf(name, "%s:capture", dev->name);
    } else if (dev->v4l2_cap.capabilities & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE)) {
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      sprintf(name, "%s:capture:mplane", dev->name);
    } else {
      E_LOG_ERROR(dev, "Video capture is not supported by device: %08x", dev->v4l2_cap.capabilities);
    }
  } else {
    buf_list = &dev->output_list;
    do_mmap = !dev->allow_dma;

    if (dev->output_list) {
      E_LOG_ERROR(dev, "The output_list is already created.");
    }

    if (dev->v4l2_cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) {
      type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
      sprintf(name, "%s:output", dev->name);
    } else if (dev->v4l2_cap.capabilities & (V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE)) {
      type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      sprintf(name, "%s:output:mplane", dev->name);
    } else {
      E_LOG_ERROR(dev, "Video output is not supported by device: %08x", dev->v4l2_cap.capabilities);
    }
  }

  *buf_list = buffer_list_open(name, dev, type, do_mmap);
  if (!*buf_list) {
    goto error;
  }

  if (buffer_list_set_format(*buf_list, width, height, format) < 0) {
    goto error;
  }

  if (buffer_list_request(*buf_list, nbufs) < 0) {
    goto error;
  }

  return 0;

error:
  buffer_list_close(*buf_list);
  *buf_list = NULL;
  return -1;
}

int device_stream(device_t *dev, bool do_on)
{
  if (dev->capture_list) {
    if (buffer_list_stream(dev->capture_list, do_on) < 0) {
      return -1;
    }
  }

  if (dev->output_list) {
    if (buffer_list_stream(dev->output_list, do_on) < 0) {
      return -1;
    }
  }

  return 0;
}

int device_consume_event(device_t *dev)
{
	struct v4l2_event event;

  if (!dev) {
    return -1;
  }

	E_LOG_DEBUG(dev, "Consuming V4L2 event ...");
  E_XIOCTL(dev, dev->fd, VIDIOC_DQEVENT, &event, "Got some V4L2 device event, but where is it?");

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
  return -1;
}

int device_video_force_key(device_t *dev)
{
  if (!dev) {
    return -1;
  }

  struct v4l2_control ctl = {0};
  ctl.id = V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME;
  ctl.value = 1;
  E_LOG_DEBUG(dev, "Forcing keyframe ...");
  E_XIOCTL(dev, dev->fd, VIDIOC_S_CTRL, &ctl, "Can't force keyframe");
  return 0;
error:
  return -1;
}

int device_set_fps(device_t *dev, int desired_fps)
{
  struct v4l2_streamparm setfps = {0};

  if (!dev) {
    return -1;
  }

  setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  setfps.parm.output.timeperframe.numerator = 1;
  setfps.parm.output.timeperframe.denominator = desired_fps;
  E_LOG_DEBUG(dev, "Configuring FPS ...");
  E_XIOCTL(dev, dev->fd, VIDIOC_S_PARM, &setfps, "Can't set FPS");
  return 0;
error:
  return -1;
}

int device_set_option(device_t *dev, const char *name, uint32_t id, int32_t value)
{
  struct v4l2_control ctl = {0};

  if (!dev) {
    return -1;
  }

  ctl.id = id;
  ctl.value = value;
  E_LOG_DEBUG(dev, "Configuring option %s (%08x) = %d", name, id, value);
  E_XIOCTL(dev, dev->fd, VIDIOC_S_CTRL, &ctl, "Can't set option %s", name);
  return 0;
error:
  return -1;
}