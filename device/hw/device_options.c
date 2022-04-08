#include "device/hw/device.h"
#include "opts/opts.h"

#include <ctype.h>

int device_set_option(device_t *dev, const char *name, uint32_t id, int32_t value)
{
  struct v4l2_control ctl = {0};

  if (!dev) {
    return -1;
  }

  ctl.id = id;
  ctl.value = value;
  E_LOG_DEBUG(dev, "Configuring option %s (%08x) = %d", name, id, value);
  E_XIOCTL(dev, dev->subdev_fd >= 0 ? dev->subdev_fd : dev->fd, VIDIOC_S_CTRL, &ctl, "Can't set option %s", name);
  return 0;
error:
  return -1;
}

void device_option_normalize_name(char *in)
{
  char *out = in;

  while (*in) {
    if (isalnum(*in)) {
      *out++ = tolower(*in++);
    } else if (isprint(*in)) {
      *out++ = '_';
      while (*++in && isprint(*in) && !isalnum(*in));
    } else {
      in++;
    }
  }

  *out++ = 0;
}

int device_set_option_string_fd(device_t *dev, int fd, const char *name, const char *value)
{
  struct v4l2_query_ext_ctrl qctrl = {0};
  struct v4l2_control ctl = {0};

  qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
  while (0 == ioctl (fd, VIDIOC_QUERY_EXT_CTRL, &qctrl)) {
    ctl.id = qctrl.id;
    qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;

    device_option_normalize_name(qctrl.name);

    if (strcmp(qctrl.name, name) != 0)
      continue;

    if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
      E_LOG_INFO(dev, "The '%s' is disabled", name);
      continue;
    } else if (qctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) {
      E_LOG_INFO(dev, "The '%s' is read-only", name);
      continue;
    }

    switch(qctrl.type) {
    case V4L2_CTRL_TYPE_INTEGER:
    case V4L2_CTRL_TYPE_BOOLEAN:
    case V4L2_CTRL_TYPE_MENU:
      ctl.value = atoi(value);
      E_LOG_VERBOSE(dev, "Configuring option %s (%08x) = %d", name, ctl.id, ctl.value);
      E_XIOCTL(dev, fd, VIDIOC_S_CTRL, &ctl, "Can't set option %s", name);
      return 1;

    default:
      E_LOG_INFO(dev, "The '%s' control type '%d' is not supported", name, qctrl.type);
      return 0;
    }
  }

  return 0;

error:
  return -1;
}

int device_set_option_string(device_t *dev, const char *option)
{
  int ret = -1;
  char *name = strdup(option);
  strcpy(name, option);

  char *value = strchr(name, '=');
  if (!value) {
    E_LOG_ERROR(dev, "Missing 'key=value': '%s'", option);
  }
  *value++ = 0;

  device_option_normalize_name(name);

  ret = device_set_option_string_fd(dev, dev->subdev_fd, name, value);
  if (ret <= 0)
    ret = device_set_option_string_fd(dev, dev->fd, name, value);
  if (ret == 0)
    E_LOG_ERROR(dev, "The '%s' was failed to find.", option);
  else if (ret < 0)
    E_LOG_ERROR(dev, "The '%s' did fail to be set.", option);

  ret = 0;

error:
  free(name);
  return ret;
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
    device_set_option_string(dev, option);
  }

  free(start);
}