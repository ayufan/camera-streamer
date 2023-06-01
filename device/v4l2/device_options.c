#include "v4l2.h"
#include "device/device.h"
#include "util/opts/log.h"
#include "util/opts/control.h"

int v4l2_device_set_option_by_id(device_t *dev, const char *name, uint32_t id, int32_t value)
{
  struct v4l2_control ctl = {0};

  if (!dev) {
    return -1;
  }

  ctl.id = id;
  ctl.value = value;
  LOG_DEBUG(dev, "Configuring option '%s' (%08x) = %d", name, id, value);
  ERR_IOCTL(dev, dev->v4l2->subdev_fd >= 0 ? dev->v4l2->subdev_fd : dev->v4l2->dev_fd, VIDIOC_S_CTRL, &ctl, "Can't set option %s", name);
  return 0;
error:
  return -1;
}

static int v4l2_device_query_control_iter_id(device_t *dev, int fd, uint32_t *id)
{
  struct v4l2_query_ext_ctrl qctrl = { .id = *id };

  if (0 != ioctl (fd, VIDIOC_QUERY_EXT_CTRL, &qctrl)) {
    return -1;
  }
  *id = qctrl.id;

  if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    LOG_VERBOSE(dev, "The '%s' is disabled", qctrl.name);
    return 0;
  } else if (qctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) {
    LOG_VERBOSE(dev, "The '%s' is read-only", qctrl.name);
    return 0;
  }

  LOG_VERBOSE(dev, "Available control: '%s' (%08x, type=%d)",
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

  for (int i = 0; i < dev->v4l2->ncontrols; i++) {
    if (device_option_is_equal(dev->v4l2->controls[i].control.name, keyp)) {
      control = &dev->v4l2->controls[i];
      break;
    }
  }

  if (!control) {
    ret = 0;
    LOG_ERROR(dev, "The '%s=%s' was failed to find.", key, value);
  }

  switch(control->control.type) {
  case V4L2_CTRL_TYPE_INTEGER:
  case V4L2_CTRL_TYPE_INTEGER_MENU:
  case V4L2_CTRL_TYPE_BOOLEAN:
    {
      struct v4l2_control ctl = {
        .id = control->control.id,
        .value = atoi(value)
      };
      LOG_INFO(dev, "Configuring option '%s' (%08x) = %d", control->control.name, ctl.id, ctl.value);
      ERR_IOCTL(dev, control->fd, VIDIOC_S_CTRL, &ctl, "Can't set option %s", control->control.name);
      ret = 1;
    }
    break;

  case V4L2_CTRL_TYPE_MENU:
    {
      struct v4l2_control ctl = {
        .id = control->control.id,
        .value = atoi(value)
      };

      for (int j = control->control.minimum; j <= control->control.maximum; j++) {
        struct v4l2_querymenu menu = {
          .id = control->control.id,
          .index = j
        };

        if (0 == ioctl(control->fd, VIDIOC_QUERYMENU, &menu)) {
          if (device_option_is_equal(valuep, (const char*)menu.name)) {
            ctl.value = j;
            break;
          }
        }
      }
      LOG_INFO(dev, "Configuring option '%s' (%08x) = %d", control->control.name, ctl.id, ctl.value);
      ERR_IOCTL(dev, control->fd, VIDIOC_S_CTRL, &ctl, "Can't set option %s", control->control.name);
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

      for ( ; (token = strsep(&string, ",")) != NULL; tokens++) {
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

      LOG_INFO(dev, "Configuring option '%s' (%08x) = [%d tokens, expected %d]",
        control->control.name, ctl.id, tokens, control->control.elems);
      ERR_IOCTL(dev, control->fd, VIDIOC_S_EXT_CTRLS, &ctrls, "Can't set option %s", control->control.name);
      ret = 1;
    }
    break;

  default:
    LOG_ERROR(dev, "The '%s' control type '%d' is not supported", control->control.name, control->control.type);
  }

error:
  free(data);
  free(keyp);
  free(valuep);
  return ret;
}

void v4l2_device_dump_options(device_t *dev, FILE *stream)
{
  fprintf(stream, "%s Options:\n", dev->name);

  for (int i = 0; i < dev->v4l2->ncontrols; i++) {
    device_v4l2_control_t *control = &dev->v4l2->controls[i];

    char name[512];
    device_option_normalize_name(control->control.name, name);

    fprintf(stream, "- available option: %s (%08x, type=%d): ",
      name, control->control.id, control->control.type);

    switch(control->control.type) {
    case V4L2_CTRL_TYPE_U8:
    case V4L2_CTRL_TYPE_U16:
    case V4L2_CTRL_TYPE_U32:
    case V4L2_CTRL_TYPE_BOOLEAN:
    case V4L2_CTRL_TYPE_INTEGER:
      fprintf(stream, "[%lld..%lld]\n", control->control.minimum, control->control.maximum);
      break;

    case V4L2_CTRL_TYPE_BUTTON:
      fprintf(stream, "button\n");
      break;

    case V4L2_CTRL_TYPE_MENU:
    case V4L2_CTRL_TYPE_INTEGER_MENU:
      fprintf(stream, "[%lld..%lld]\n", control->control.minimum, control->control.maximum);

      for (int j = control->control.minimum; j <= control->control.maximum; j++) {
        struct v4l2_querymenu menu = {
          .id = control->control.id,
          .index = j
        };

        if (0 == ioctl(control->fd, VIDIOC_QUERYMENU, &menu)) {
          if (control->control.type == V4L2_CTRL_TYPE_MENU) {
            fprintf(stream, "\t\t%d: %s\n", j, menu.name);
          } else {
            fprintf(stream, "\t\t%d: %lld\n", j, menu.value);
          }
        }
      }
      break;

    case V4L2_CTRL_TYPE_STRING:
      fprintf(stream, "(string)\n");
      break;

    default:
      fprintf(stream, "(unsupported)\n");
      break;
    }
  }
  fprintf(stream, "\n");
}

int v4l2_device_dump_options2(device_t *dev, device_option_fn fn, void *opaque)
{
  int n = 0;

  for (int i = 0; i < dev->v4l2->ncontrols; i++) {
    device_v4l2_control_t *control = &dev->v4l2->controls[i];

    device_option_t opt = {
      .control_id = control->control.id,
      .elems = control->control.elems,
    };
    strcpy(opt.name, control->control.name);

    char buf[8192];

    struct v4l2_ext_control ext_control = {
      .id = control->control.id,
      .size = sizeof(buf),
      .ptr = buf,
    };

    struct v4l2_ext_controls ext_controls = {
      .ctrl_class = 0,
      .which = V4L2_CTRL_WHICH_CUR_VAL,
      .count = 1,
      .controls = &ext_control
    };

    bool ext_control_valid = ioctl(control->fd, VIDIOC_G_EXT_CTRLS, &ext_controls) == 0;

    switch(control->control.type) {
    case V4L2_CTRL_TYPE_U8:
      opt.type = device_option_type_u8;
      snprintf(opt.description, sizeof(opt.description), "[%02x..%02x]",
        (__u8)control->control.minimum & 0xFF, (__u8)control->control.maximum & 0xFF);
      if (ext_control_valid) {
        int n = 0;
        for (int i = 0; i < ext_control.size; i++) {
          if (n + 3 >= sizeof(opt.value)) break;
          if (i) opt.value[n++] = ' ';
          n += sprintf(opt.value + n, "%02x", ext_control.p_u8[i]&0xFF);
        }
      }
      break;

    case V4L2_CTRL_TYPE_U16:
      opt.type = device_option_type_u16;
      snprintf(opt.description, sizeof(opt.description), "[%04x..%04x]",
        (__u16)control->control.minimum & 0xFFFF, (__u16)control->control.maximum & 0xFFFF);
      if (ext_control_valid) {
        int n = 0;
        for (int i = 0; i < ext_control.size; i++) {
          if (n + 5 >= sizeof(opt.value)) break;
          if (i) opt.value[n++] = ' ';
          n += sprintf(opt.value + n, "%04x", ext_control.p_u16[i]&0xFFFF);
        }
      }
      break;

    case V4L2_CTRL_TYPE_U32:
      opt.type = device_option_type_u32;
      snprintf(opt.description, sizeof(opt.description), "[%lld..%lld]",
        control->control.minimum, control->control.maximum);
      if (ext_control_valid) {
        int n = 0;
        for (int i = 0; i < ext_control.size; i++) {
          if (n + 9 >= sizeof(opt.value)) break;
          if (i) opt.value[n++] = ' ';
          n += sprintf(opt.value + n, "%08x", ext_control.p_u32[i]);
        }
      }
      break;

    case V4L2_CTRL_TYPE_BOOLEAN:
      opt.type = device_option_type_bool;
      snprintf(opt.description, sizeof(opt.description), "[%lld..%lld]",
        control->control.minimum, control->control.maximum);
      if (ext_control_valid)
        snprintf(opt.value, sizeof(opt.value), "%d", ext_control.value ? 1 : 0);
      break;

    case V4L2_CTRL_TYPE_INTEGER:
      opt.type = device_option_type_integer;
      snprintf(opt.description, sizeof(opt.description), "[%lld..%lld]",
        control->control.minimum, control->control.maximum);
      if (ext_control_valid)
        snprintf(opt.value, sizeof(opt.value),  "%d", ext_control.value);
      break;

    case V4L2_CTRL_TYPE_INTEGER64:
      opt.type = device_option_type_integer64;
      snprintf(opt.description, sizeof(opt.description), "[%lld..%lld]",
        control->control.minimum, control->control.maximum);
      if (ext_control_valid)
        snprintf(opt.value, sizeof(opt.value), "%lld", ext_control.value64);
      break;

    case V4L2_CTRL_TYPE_BUTTON:
      opt.type = device_option_type_bool;
      snprintf(opt.description, sizeof(opt.description), "button");
      break;

    case V4L2_CTRL_TYPE_MENU:
    case V4L2_CTRL_TYPE_INTEGER_MENU:
      opt.type = device_option_type_integer;
      snprintf(opt.description, sizeof(opt.description), "[%lld..%lld]",
        control->control.minimum, control->control.maximum);

      if (ext_control_valid)
        snprintf(opt.value, sizeof(opt.value), "%d", ext_control.value);

      for (int j = control->control.minimum; j <= control->control.maximum; j++) {
        struct v4l2_querymenu menu = {
          .id = control->control.id,
          .index = j
        };

        if (0 != ioctl(control->fd, VIDIOC_QUERYMENU, &menu))
          continue;

        if (opt.menu_items >= MAX_DEVICE_OPTION_MENU) {
          opt.flags.invalid = true;
          break;
        }

        device_option_menu_t *opt_menu = &opt.menu[opt.menu_items++];
        opt_menu->id = j;

        if (control->control.type == V4L2_CTRL_TYPE_MENU) {
          snprintf(opt_menu->name, sizeof(opt_menu->name), "%s", menu.name);
          if (ext_control_valid && opt_menu->id == ext_control.value)
            snprintf(opt.value, sizeof(opt.value), "%s", menu.name);
        } else {
          snprintf(opt_menu->name, sizeof(opt_menu->name), "%lld", menu.value);
        }
      }
      break;

    case V4L2_CTRL_TYPE_STRING:
      opt.type = device_option_type_string;
      if (ext_control_valid)
        snprintf(opt.value, sizeof(opt.value), "%s", ext_control.string);
      break;

    default:
      opt.flags.invalid = true; // unsupported
      break;
    }

    if (opt.type) {
      int ret = fn(&opt, opaque);
      if (ret < 0)
        return ret;
      n++;
    }
  }

  return n;
}
