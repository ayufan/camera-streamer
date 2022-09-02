#include "v4l2.h"
#include "device/device_list.h"
#include "util/opts/log.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

static void device_list_read_formats(int fd, device_info_formats_t *formats, enum v4l2_buf_type buf_type)
{
  for (int i = 0; ; ++i) {
    struct v4l2_fmtdesc format_desc;
    memset(&format_desc, 0, sizeof(format_desc));
    format_desc.type = (enum v4l2_buf_type) buf_type;
    format_desc.index = i;

    if (-1 == ioctl(fd, VIDIOC_ENUM_FMT, &format_desc)) {
      break;
    }

    formats->n++;
    formats->formats = realloc(formats->formats, sizeof(formats->formats[0]) * formats->n);
    formats->formats[formats->n - 1] = format_desc.pixelformat;
  }
}

static bool device_list_read_dev(device_info_t *info, const char *name)
{
  asprintf(&info->path, "/dev/%s", name);

  int fd = open(info->path, O_RDWR|O_NONBLOCK);
  if (fd < 0) {
    LOG_ERROR(NULL, "Can't open device: %s", info->path);
  }

  struct v4l2_capability v4l2_cap;
  ERR_IOCTL(info, fd, VIDIOC_QUERYCAP, &v4l2_cap, "Can't query device capabilities");
  info->name = strdup((const char *)v4l2_cap.card);

  if (!(v4l2_cap.capabilities & V4L2_CAP_STREAMING)) {
    LOG_VERBOSE(info, "Device (%s) does not support streaming (skipping)", info->path);
    goto error;
  } else if ((v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) && !(v4l2_cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
    info->camera = true;
  } else if (!(v4l2_cap.capabilities & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE))) {
    LOG_VERBOSE(info, "Device (%s) does not support capture (skipping)", info->path);
    goto error;
  } else if (!(v4l2_cap.capabilities & (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE))) {
    LOG_VERBOSE(info, "Device (%s) does not support output (skipping)", info->path);
    goto error;
  } else if ((v4l2_cap.capabilities & V4L2_CAP_VIDEO_M2M) || (v4l2_cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE)) {
    info->m2m = true;
  }

  device_list_read_formats(fd, &info->capture_formats, V4L2_BUF_TYPE_VIDEO_CAPTURE);
  device_list_read_formats(fd, &info->capture_formats, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  device_list_read_formats(fd, &info->output_formats, V4L2_BUF_TYPE_VIDEO_OUTPUT);
  device_list_read_formats(fd, &info->output_formats, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  close(fd);

  return true;

error:
  free(info->name);
  free(info->path);
  close(fd);
  return false;
}

device_list_t *device_list_v4l2()
{
  DIR *dev = opendir("/dev");
  if (!dev) {
    return NULL;
  }

  device_list_t *list = calloc(1, sizeof(device_list_t));
  struct dirent *ent;

  while ((ent = readdir(dev)) != NULL) {
    if (strstr(ent->d_name, "video") != ent->d_name) {
      continue;
    }

    device_info_t info = {NULL};
    if (device_list_read_dev(&info, ent->d_name)) {
      list->ndevices++;
      list->devices = realloc(list->devices, sizeof(info) * list->ndevices);
      list->devices[list->ndevices-1] = info;
    }
  }

  closedir(dev);

  return list;
}
