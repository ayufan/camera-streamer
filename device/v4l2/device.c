#include "v4l2.h"
#include "device/device.h"
#include "opts/log.h"

int v4l2_device_open(device_t *dev)
{
  dev->v4l2 = calloc(1, sizeof(device_v4l2_t));
  dev->v4l2->dev_fd = -1;
  dev->v4l2->subdev_fd = -1;

  dev->v4l2->dev_fd = open(dev->path, O_RDWR|O_NONBLOCK);
  if (dev->v4l2->dev_fd < 0) {
		E_LOG_ERROR(dev, "Can't open device: %s", dev->path);
    goto error;
  }

	E_LOG_INFO(dev, "Device path=%s fd=%d opened", dev->path, dev->v4l2->dev_fd);

	E_LOG_DEBUG(dev, "Querying device capabilities ...");
  struct v4l2_capability v4l2_cap;
  E_XIOCTL(dev, dev->v4l2->dev_fd, VIDIOC_QUERYCAP, &v4l2_cap, "Can't query device capabilities");

	if (!(v4l2_cap.capabilities & V4L2_CAP_STREAMING)) {
		E_LOG_ERROR(dev, "Device doesn't support streaming IO");
	}

  strcpy(dev->bus_info, v4l2_cap.bus_info);
  dev->v4l2->subdev_fd = v4l2_device_open_v4l2_subdev(dev, 0);
  return 0;

error:
  return -1;
}

void v4l2_device_close(device_t *dev)
{
  if (dev->v4l2->subdev_fd >= 0) {
    close(dev->v4l2->subdev_fd);
  }

  if(dev->v4l2->dev_fd >= 0) {
    close(dev->v4l2->dev_fd);
  }

  free(dev->v4l2);
  dev->v4l2 = NULL;
}

int v4l2_device_set_decoder_start(device_t *dev, bool do_on)
{
  struct v4l2_decoder_cmd cmd = {0};

  cmd.cmd = do_on ? V4L2_DEC_CMD_START : V4L2_DEC_CMD_STOP;

  E_LOG_DEBUG(dev, "Setting decoder state %s...", do_on ? "Start" : "Stop");
  E_XIOCTL(dev, dev->v4l2->dev_fd, VIDIOC_DECODER_CMD, &cmd, "Cannot set decoder state");

  return 0;

error:
  return -1;
}

int v4l2_device_video_force_key(device_t *dev)
{
  struct v4l2_control ctl = {0};
  ctl.id = V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME;
  ctl.value = 1;
  E_LOG_DEBUG(dev, "Forcing keyframe ...");
  E_XIOCTL(dev, dev->v4l2->dev_fd, VIDIOC_S_CTRL, &ctl, "Can't force keyframe");
  return 0;

error:
  return -1;
}

int v4l2_device_set_fps(device_t *dev, int desired_fps)
{
  struct v4l2_streamparm setfps = {0};

  if (!dev) {
    return -1;
  }

  setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  setfps.parm.output.timeperframe.numerator = 1;
  setfps.parm.output.timeperframe.denominator = desired_fps;
  E_LOG_DEBUG(dev, "Configuring FPS ...");
  E_XIOCTL(dev, dev->v4l2->dev_fd, VIDIOC_S_PARM, &setfps, "Can't set FPS");
  return 0;
error:
  return -1;
}
