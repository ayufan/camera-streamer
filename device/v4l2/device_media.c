#include "v4l2.h"

#include "device/device.h"
#include "opts/log.h"

#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <linux/media.h>

int v4l2_device_open_media_device(device_t *dev)
{
  struct stat st;
  if (fstat(dev->v4l2->dev_fd, &st) < 0) {
    LOG_VERBOSE(dev, "Cannot get fstat");
    return -1;
  }
  if (~st.st_mode & S_IFCHR) {
    LOG_VERBOSE(dev, "FD is not char");
    return -1;
  }

  char path[256];
  sprintf(path, "/sys/dev/char/%d:%d/device", major(st.st_rdev), minor(st.st_rdev));

  struct dirent **namelist;
  int n = scandir(path, &namelist, NULL, NULL);
  if (n < 0) {
    LOG_VERBOSE(dev, "Cannot scan: %s", path);
    return -1;
  }

  int ret = -1;
  while (n--) {
    if (ret == -1 && strstr(namelist[n]->d_name, "media") == namelist[n]->d_name) {
      path[0] = 0;
      sprintf(path, "/dev/%s", namelist[n]->d_name);
      ret = open(path, O_RDWR);
      if (ret >= 0) {
        LOG_VERBOSE(dev, "Opened '%s' (fd=%d)", path, ret);
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
    LOG_VERBOSE(dev, "Cannot find media controller");
    return -1;
  }

  for (;;) {
    struct media_entity_desc entity = {
      .id	= MEDIA_ENT_ID_FLAG_NEXT | last_id,
    };

    int rc = ioctl(media_fd, MEDIA_IOC_ENUM_ENTITIES, &entity);
    if (rc < 0 && errno == EINVAL) {
      break;
    }

    if (rc < 0) {
      goto error;
    }

    last_id = entity.id;
    char path[256];
    sprintf(path, "/sys/dev/char/%d:%d", entity.dev.major, entity.dev.minor);

    char link[256];
    if ((rc = readlink(path, link, sizeof(link)-1)) < 0) {
      LOG_VERBOSE(dev, "Cannot readlink '%s'", path);
    }
    link[rc] = 0;

    char *last = strrchr(link, '/');
    if (!last) {
      LOG_VERBOSE(dev, "Link '%s' for '%s' does not end with '/'", link, path);
      goto error;
    }

    if (strstr(last, "/v4l-subdev") != last) {
      LOG_VERBOSE(dev, "Link '%s' does not contain '/v4l-subdev'", link, path);
      goto error;
    }

    sprintf(path, "/dev%s", last);
    ret = open(path, O_RDWR);
    if (ret < 0) {
      LOG_ERROR(dev, "Cannot open '%s' (ret=%d)", path, ret);
      goto error;
    }

    LOG_VERBOSE(dev, "Opened '%s' (fd=%d)", path, ret);
    break;
  }

error:
  close(media_fd);
  return ret;
}

int v4l2_device_set_pad_format(device_t *dev, unsigned width, unsigned height, unsigned format)
{
  struct v4l2_subdev_format fmt = {0};

  if (dev->v4l2->subdev_fd < 0) {
    return -1;
  }

  fmt.pad = 0;
  fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
  fmt.format.code = format;
  fmt.format.width = width;
  fmt.format.height = height;
  fmt.format.colorspace = V4L2_COLORSPACE_RAW;
  fmt.format.field = V4L2_FIELD_ANY;

  LOG_DEBUG(dev, "Configuring mpad %d (subdev_fd=%d)...", fmt.pad, dev->v4l2->subdev_fd);
  ERR_IOCTL(dev, dev->v4l2->subdev_fd, VIDIOC_SUBDEV_S_FMT, &fmt, "Can't configure mpad %d (subdev_fd=%d)", fmt.pad, dev->v4l2->subdev_fd);
  return 0;

error:
  return -1;
}
