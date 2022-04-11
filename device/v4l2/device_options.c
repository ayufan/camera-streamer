#include "v4l2.h"
#include "device/device.h"
#include "opts/log.h"
#include "opts/control.h"

int v4l2_device_set_option_by_id(device_t *dev, const char *name, uint32_t id, int32_t value)
{
  struct v4l2_control ctl = {0};

  if (!dev) {
    return -1;
  }

  ctl.id = id;
  ctl.value = value;
  E_LOG_DEBUG(dev, "Configuring option %s (%08x) = %d", name, id, value);
  E_XIOCTL(dev, dev->v4l2->subdev_fd >= 0 ? dev->v4l2->subdev_fd : dev->v4l2->dev_fd, VIDIOC_S_CTRL, &ctl, "Can't set option %s", name);
  return 0;
error:
  return -1;
}

static int v4l2_device_query_control_iter_id(device_t *dev, int fd, uint32_t *id)
{
  struct v4l2_query_ext_ctrl qctrl = { .id = *id };
  void *data = NULL;

  if (0 != ioctl (fd, VIDIOC_QUERY_EXT_CTRL, &qctrl)) {
    return -1;
  }
  *id = qctrl.id;

  device_option_normalize_name(qctrl.name, qctrl.name);

  if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    E_LOG_VERBOSE(dev, "The '%s' is disabled", qctrl.name);
    return 0;
  } else if (qctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) {
    E_LOG_VERBOSE(dev, "The '%s' is read-only", qctrl.name);
    return 0;
  }

  E_LOG_VERBOSE(dev, "Available control: %s (%08x, type=%d)",
    qctrl.name, qctrl.id, qctrl.type);

  dev->v4l2->controls = reallocarray(dev->v4l2->controls, dev->v4l2->ncontrols+1, sizeof(device_v4l2_control_t));
  dev->v4l2->controls[dev->v4l2->ncontrols++] = (device_v4l2_control_t){
    .fd = fd,
    .control = qctrl
  };

  return 0;
}

void v4l2_device_query_controls(device_t *dev, int fd)
{
  if (fd < 0)
    return;

  int ret = 0;
  uint32_t id = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
  while ((ret = v4l2_device_query_control_iter_id(dev, fd, &id)) == 0) {
    id |= V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
  }
}

int v4l2_device_set_option(device_t *dev, const char *key, const char *value)
{
  char *keyp = strdup(key);
  char *valuep = strdup(value);
  void *data = NULL;
  device_v4l2_control_t *control = NULL;
  int ret = -1;

  device_option_normalize_name(keyp, keyp);

  for (int i = 0; i < dev->v4l2->ncontrols; i++) {
    if (strcmp(dev->v4l2->controls[i].control.name, keyp) == 0) {
      control = &dev->v4l2->controls[i];
      break;
    }
  }

  if (!control) {
    ret = 0;
    E_LOG_ERROR(dev, "The '%s=%s' was failed to find.", key, value);
  }

  switch(control->control.type) {
  case V4L2_CTRL_TYPE_INTEGER:
  case V4L2_CTRL_TYPE_BOOLEAN:
  case V4L2_CTRL_TYPE_MENU:
    {
      struct v4l2_control ctl = {
        .id = control->control.id,
        .value = atoi(value)
      };
      E_LOG_INFO(dev, "Configuring option %s (%08x) = %d", control->control.name, ctl.id, ctl.value);
      E_XIOCTL(dev, control->fd, VIDIOC_S_CTRL, &ctl, "Can't set option %s", control->control.name);
      ret = 1;
    }
    break;

  case V4L2_CTRL_TYPE_U8:
  case V4L2_CTRL_TYPE_U16:
  case V4L2_CTRL_TYPE_U32:
    {
      struct v4l2_ext_control ctl = {
        .id = control->control.id,
        .size = control->control.elem_size * control->control.elems,
        .ptr = data = calloc(control->control.elems, control->control.elem_size)
      };
      struct v4l2_ext_controls ctrls = {
        .count = 1,
        .ctrl_class = V4L2_CTRL_ID2CLASS(control->control.id),
        .controls = &ctl,
      };

      char *string = valuep;
      char *token;
      int tokens = 0;

      for ( ; token = strsep(&string, ","); tokens++) {
        if (tokens >= control->control.elems)
          continue;

        switch(control->control.type) {
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

      E_LOG_INFO(dev, "Configuring option %s (%08x) = [%d tokens, expected %d]",
        control->control.name, ctl.id, tokens, control->control.elems);
      E_XIOCTL(dev, control->fd, VIDIOC_S_EXT_CTRLS, &ctrls, "Can't set option %s", control->control.name);
      ret = 1;
    }
    break;

  default:
    E_LOG_ERROR(dev, "The '%s' control type '%d' is not supported", control->control.name, control->control.type);
  }

error:
  free(data);
  free(keyp);
  free(valuep);
  return ret;
}
