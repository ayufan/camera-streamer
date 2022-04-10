#include "device/v4l2/v4l2.h"
#include "device/device.h"
#include "device/hw/v4l2.h"

#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <linux/media.h>

int v4l2_device_open_media_device(device_t *dev)
{
  struct stat st;
  if (fstat(dev->fd, &st) < 0) {
    E_LOG_ERROR(dev, "Cannot get fstat");
    return -1;
  }
  if (~st.st_mode & S_IFCHR) {
    E_LOG_ERROR(dev, "FD is not char");
    return -1;
  }

  char path[256];
  sprintf(path, "/sys/dev/char/%d:%d/device", major(st.st_rdev), minor(st.st_rdev));

  struct dirent **namelist;
  int n = scandir(path, &namelist, NULL, NULL);
  if (n < 0) {
    E_LOG_ERROR(dev, "Cannot scan: %s", path);
    return -1;
  }

  int ret = -1;
  while (n--) {
    if (ret == -1 && strstr(namelist[n]->d_name, "media") == namelist[n]->d_name) {
      path[0] = 0;
      sprintf(path, "/dev/%s", namelist[n]->d_name);
      ret = open(path, O_RDWR);
      if (ret >= 0) {
        E_LOG_VERBOSE(dev, "Opened '%s' (fd=%d)", path, ret);
      }
    }

    free(namelist[n]);
  }
  free(namelist);

error:
  return ret;
}

int v4l2_device_open_v4l2_subdev(device_t *dev, int subdev)
{
  int media_fd = -1;
	unsigned int last_id = 0;
  int ret = -1;

  media_fd = v4l2_device_open_media_device(dev);
  if (media_fd < 0) {
    E_LOG_ERROR(dev, "Cannot find media controller");
  }

  for (;;) {
    struct media_entity_desc entity = {
      .id	= MEDIA_ENT_ID_FLAG_NEXT | last_id,
    };

    int rc = ioctl(media_fd, MEDIA_IOC_ENUM_ENTITIES, &entity);
    if (rc < 0 && errno == EINVAL)
      break;

    if (rc < 0) {
      goto error;
    }

    last_id = entity.id;
    char path[256];
    sprintf(path, "/sys/dev/char/%d:%d", entity.dev.major, entity.dev.minor);

    char link[256];
    if (readlink(path, link, sizeof(link)) < 0) {
      E_LOG_ERROR(dev, "Cannot readlink '%s'", path);
      goto error;
    }

    char * last = strrchr(link, '/');
    if (!last) {
      E_LOG_ERROR(dev, "Link '%s' for '%s' does not end with '/'", link, path);
      goto error;
    }

    if (strstr(last, "/v4l-subdev") != last) {
      goto error;
    }

    sprintf(path, "/dev%s", last);
    ret = open(path, O_RDWR);
    if (ret >= 0) {
      E_LOG_INFO(dev, "Opened '%s' (fd=%d)", path, ret);
    }
    break;
  }

error:
  close(media_fd);
  return ret;
}

int v4l2_device_set_pad_format(device_t *dev, unsigned width, unsigned height, unsigned format)
{
  struct v4l2_subdev_format fmt = {0};

  if (dev->subdev_fd < 0) {
    return -1;
  }

  fmt.pad = 0;
  fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
  fmt.format.code = format;
  fmt.format.width = width;
  fmt.format.height = height;
  fmt.format.colorspace = V4L2_COLORSPACE_RAW;
  fmt.format.field = V4L2_FIELD_ANY;

  E_LOG_DEBUG(dev, "Configuring mpad %d (subdev_fd=%d)...", fmt.pad, dev->subdev_fd);
  E_XIOCTL(dev, dev->subdev_fd, VIDIOC_SUBDEV_S_FMT, &fmt, "Can't configure mpad %d (subdev_fd=%d)", fmt.pad, dev->subdev_fd);
  return 0;

error:
  return -1;
}
