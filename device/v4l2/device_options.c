#include "device/v4l2/v4l2.h"
#include "device/hw/v4l2.h"
#include "device/device.h"

#include <ctype.h>

int v4l2_device_set_option_by_id(device_t *dev, const char *name, uint32_t id, int32_t value)
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

void v4l2_device_option_normalize_name(char *in)
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

static int v4l2_device_set_option_string_fd_iter_id(device_t *dev, int fd, uint32_t *id, char *name, char *value)
{
  struct v4l2_query_ext_ctrl qctrl = { .id = *id };
  void *data = NULL;

  if (0 != ioctl (fd, VIDIOC_QUERY_EXT_CTRL, &qctrl)) {
    *id = 0;
    return 0;
  }
  *id = qctrl.id;

  v4l2_device_option_normalize_name(qctrl.name);

  if (strcmp(qctrl.name, name) != 0)
    return 0;

  if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    E_LOG_INFO(dev, "The '%s' is disabled", name);
    return 0;
  } else if (qctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) {
    E_LOG_INFO(dev, "The '%s' is read-only", name);
    return 0;
  }

  switch(qctrl.type) {
  case V4L2_CTRL_TYPE_INTEGER:
  case V4L2_CTRL_TYPE_BOOLEAN:
  case V4L2_CTRL_TYPE_MENU:
    {
      struct v4l2_control ctl = {
        .id = *id,
        .value = atoi(value)
      };
      E_LOG_INFO(dev, "Configuring option %s (%08x) = %d", name, ctl.id, ctl.value);
      E_XIOCTL(dev, fd, VIDIOC_S_CTRL, &ctl, "Can't set option %s", name);
    }
    return 1;

  case V4L2_CTRL_TYPE_U8:
  case V4L2_CTRL_TYPE_U16:
  case V4L2_CTRL_TYPE_U32:
    {
      struct v4l2_ext_control ctl = {
        .id = *id,
        .size = qctrl.elem_size * qctrl.elems,
        .ptr = data = calloc(qctrl.elems, qctrl.elem_size)
      };
      struct v4l2_ext_controls ctrls = {
        .count = 1,
        .ctrl_class = V4L2_CTRL_ID2CLASS(*id),
        .controls = &ctl,
      };

      char *string = value;
      char *token;
      int tokens = 0;

      for ( ; token = strsep(&string, ","); tokens++) {
        if (tokens >= qctrl.elems)
          continue;

        switch(qctrl.type) {
        case V4L2_CTRL_TYPE_U8:
          ctl.p_u8[tokens] = atoi(token);
          break;

        case V4L2_CTRL_TYPE_U16:
          ctl.p_u16[tokens] = atoi(token);
          break;

        case V4L2_CTRL_TYPE_U32:
          ctl.p_u32[tokens] = atoi(token);
          break;
        }
      }

      E_LOG_INFO(dev, "Configuring option %s (%08x) = [%d tokens, expected %d]", name, ctl.id, tokens, qctrl.elems);
      E_XIOCTL(dev, fd, VIDIOC_S_EXT_CTRLS, &ctrls, "Can't set option %s", name);
      free(data);
    }
    return 1;

  default:
    E_LOG_INFO(dev, "The '%s' control type '%d' is not supported", name, qctrl.type);
    return -1;
  }

error:
  free(data);
  return -1;
}

static int v4l2_device_set_option_string_fd(device_t *dev, int fd, char *name, char *value)
{
  int ret = 0;

  uint32_t id = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
  while ((ret = v4l2_device_set_option_string_fd_iter_id(dev, fd, &id, name, value)) == 0 && id) {
    id |= V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
  }

  return ret;
}

int v4l2_device_set_option(device_t *dev, const char *key, const char *value)
{
  char *keyp = strdup(key);
  char *valuep = strdup(value);

  int ret = 0;

  v4l2_device_option_normalize_name(keyp);

  if (dev->subdev_fd >= 0)
    ret = v4l2_device_set_option_string_fd(dev, dev->subdev_fd, keyp, valuep);
  if (ret <= 0)
    ret = v4l2_device_set_option_string_fd(dev, dev->fd, keyp, valuep);

  free(keyp);
  free(valuep);

  if (ret == 0)
    E_LOG_ERROR(dev, "The '%s=%s' was failed to find.", key, value);
  else if (ret < 0)
    E_LOG_ERROR(dev, "The '%s=%s' did fail to be set.", key, value);

  return 0;

error:
  return -1;
}
