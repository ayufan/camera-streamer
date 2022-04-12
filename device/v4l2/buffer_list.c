#include "v4l2.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "opts/log.h"
#include "opts/fourcc.h"

int v4l2_buffer_list_open(buffer_list_t *buf_list)
{
  device_t *dev = buf_list->dev;

  buf_list->v4l2 = calloc(1, sizeof(buffer_list_v4l2_t));
  buf_list->v4l2->dev_fd = dev->v4l2->dev_fd;
  if (buf_list->v4l2->dev_fd < 0) {
    return -1;
  }

  if (buf_list->path) {
    buf_list->v4l2->dev_fd = open(buf_list->path, O_RDWR|O_NONBLOCK);
    if (buf_list->v4l2->dev_fd < 0) {
      LOG_ERROR(buf_list, "Can't open device: %s", buf_list->path);
      return -1;
    }

	  LOG_INFO(buf_list, "Device path=%s fd=%d opened", buf_list->path, buf_list->v4l2->dev_fd);
  }

  struct v4l2_capability v4l2_cap;
  ERR_IOCTL(dev, buf_list->v4l2->dev_fd, VIDIOC_QUERYCAP, &v4l2_cap, "Can't query device capabilities");

  if (buf_list->do_capture) {
     if (v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
      buf_list->v4l2->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    } else if (v4l2_cap.capabilities & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE)) {
      buf_list->v4l2->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      buf_list->v4l2->do_mplanes = true;
    } else {
      LOG_ERROR(buf_list, "Video capture is not supported by device: %08x", v4l2_cap.capabilities);
    }
  } else {
    if (v4l2_cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) {
      buf_list->v4l2->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    } else if (v4l2_cap.capabilities & (V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE)) {
      buf_list->v4l2->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      buf_list->v4l2->do_mplanes = true;
    } else {
      LOG_ERROR(buf_list, "Video output is not supported by device: %08x", v4l2_cap.capabilities);
    }
  }

  // Add suffix for debug purposes
  if (v4l2_cap.capabilities & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE)) {
#define MPLANE_SUFFIX ":mplane"
    buf_list->name = realloc(buf_list->name, strlen(buf_list->name) + strlen(MPLANE_SUFFIX) + 1);
    strcat(buf_list->name, MPLANE_SUFFIX);
  }

	struct v4l2_format v4l2_fmt = { 0 };
  v4l2_fmt.type = buf_list->v4l2->type;

  buffer_format_t fmt = buf_list->fmt;

  // JPEG is in 16x16 blocks (shrink image to fit) (but adapt to 32x32)
  // And ISP output
  if (strstr(buf_list->name, "JPEG") || strstr(buf_list->name, "H264") || buf_list->do_capture && strstr(buf_list->name, "ISP")) {
    buffer_format_t org_fmt = buf_list->fmt;
    fmt.width = shrink_to_block(fmt.width, 32);
    fmt.height = shrink_to_block(fmt.height, 32);
    LOG_VERBOSE(buf_list, "Adapting size to 32x32 block: %dx%d vs %dx%d", org_fmt.width, org_fmt.height, fmt.width, fmt.height);
  }

  if (fmt.format == V4L2_PIX_FMT_H264) {
    fmt.bytesperline = 0;
  }

  LOG_DEBUG(buf_list, "Get current format ...");
  ERR_IOCTL(buf_list, buf_list->v4l2->dev_fd, VIDIOC_G_FMT, &v4l2_fmt, "Can't set format");

  if (buf_list->v4l2->do_mplanes) {
    v4l2_fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_JPEG;
    if (fmt.width)
      v4l2_fmt.fmt.pix_mp.width = fmt.width;
    if (fmt.height)
      v4l2_fmt.fmt.pix_mp.height = fmt.height;
    if (fmt.format)
      v4l2_fmt.fmt.pix_mp.pixelformat = fmt.format;
    v4l2_fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    v4l2_fmt.fmt.pix_mp.num_planes = 1;
    v4l2_fmt.fmt.pix_mp.plane_fmt[0].bytesperline = fmt.bytesperline;
    //v4l2_fmt.fmt.pix_mp.plane_fmt[0].sizeimage = bytesperline * orig_height;
  } else {
    v4l2_fmt.fmt.pix.colorspace = V4L2_COLORSPACE_RAW;
    if (fmt.width)
      v4l2_fmt.fmt.pix.width = fmt.width;
    if (fmt.height)
      v4l2_fmt.fmt.pix.height = fmt.height;
    if (fmt.format)
      v4l2_fmt.fmt.pix.pixelformat = fmt.format;
    v4l2_fmt.fmt.pix.field = V4L2_FIELD_ANY;
    v4l2_fmt.fmt.pix.bytesperline = fmt.bytesperline;
    //v4l2_fmt.fmt.pix.sizeimage = bytesperline * orig_height;
  }

  LOG_DEBUG(buf_list, "Configuring format (%s)...", fourcc_to_string(fmt.format).buf);
  ERR_IOCTL(buf_list, buf_list->v4l2->dev_fd, VIDIOC_S_FMT, &v4l2_fmt, "Can't set format");

  if (buf_list->v4l2->do_mplanes) {
    buf_list->fmt.width = v4l2_fmt.fmt.pix_mp.width;
    buf_list->fmt.height = v4l2_fmt.fmt.pix_mp.height;
    buf_list->fmt.format = v4l2_fmt.fmt.pix_mp.pixelformat;
    buf_list->fmt.bytesperline = v4l2_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
  } else {
    buf_list->fmt.width = v4l2_fmt.fmt.pix.width;
    buf_list->fmt.height = v4l2_fmt.fmt.pix.height;
    buf_list->fmt.format = v4l2_fmt.fmt.pix.pixelformat;
    buf_list->fmt.bytesperline = v4l2_fmt.fmt.pix.bytesperline;
  }

  if (buf_list->fmt.width != fmt.width || buf_list->fmt.height != fmt.height) {
    if (fmt.bytesperline) {
      LOG_ERROR(buf_list, "Requested resolution=%ux%u is unavailable. Got %ux%u. "
        "Consider using the `-camera-high_res_factor=2` or `-camera-low_res_factor=3`",
        fmt.width, fmt.height, buf_list->fmt.width, buf_list->fmt.height);
    } else {
      LOG_INFO(buf_list, "Requested resolution=%ux%u is unavailable. Got %ux%u. Accepted",
        fmt.width, fmt.height, buf_list->fmt.width, buf_list->fmt.height);
    }
  }

	if (fmt.format && buf_list->fmt.format != fmt.format) {
		LOG_ERROR(buf_list, "Could not obtain the requested format=%s; driver gave us %s",
			fourcc_to_string(fmt.format).buf,
			fourcc_to_string(buf_list->fmt.format).buf);
	}

  if (fmt.bytesperline > 0 && buf_list->fmt.bytesperline != fmt.bytesperline) {
		LOG_ERROR(buf_list, "Requested bytesperline=%u. Got %u.",
      fmt.bytesperline, buf_list->fmt.bytesperline);
  }

  // Some devices require setting pad size via media-controller
  if (buf_list->do_capture) {
    v4l2_device_set_pad_format(dev, fmt.width, fmt.height, fmt.format);
  }

	struct v4l2_requestbuffers v4l2_req = {0};
	v4l2_req.count = fmt.nbufs;
	v4l2_req.type = buf_list->v4l2->type;
	v4l2_req.memory = buf_list->do_mmap ? V4L2_MEMORY_MMAP : V4L2_MEMORY_DMABUF;

	LOG_DEBUG(buf_list, "Requesting %u buffers", v4l2_req.count);

	ERR_IOCTL(buf_list, buf_list->v4l2->dev_fd, VIDIOC_REQBUFS, &v4l2_req, "Can't request buffers");
	if (v4l2_req.count < 1) {
		LOG_ERROR(buf_list, "Insufficient buffer memory: %u", v4l2_req.count);
	}

	LOG_DEBUG(buf_list, "Got %u buffers", v4l2_req.count);
  buf_list->fmt.nbufs = v4l2_req.count;
  return v4l2_req.count;

error:
  return -1;
}

int v4l2_buffer_list_set_stream(buffer_list_t *buf_list, bool do_on)
{
	enum v4l2_buf_type type = buf_list->v4l2->type;
  ERR_IOCTL(buf_list, buf_list->v4l2->dev_fd, do_on ? VIDIOC_STREAMON : VIDIOC_STREAMOFF, &type, "Cannot set streaming state");

  return 0;
error:
  return -1;
}

void v4l2_buffer_list_close(buffer_list_t *buf_list) {
  if (!buf_list->v4l2)
    return;

  if (buf_list->dev->v4l2->dev_fd != buf_list->v4l2->dev_fd) {
    close(buf_list->v4l2->dev_fd);
  }
  free(buf_list->v4l2);
  buf_list->v4l2 = NULL;
}
