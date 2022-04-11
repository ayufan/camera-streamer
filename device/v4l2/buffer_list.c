#include "v4l2.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "opts/log.h"
#include "opts/fourcc.h"

int v4l2_buffer_list_open(buffer_list_t *buf_list, unsigned width, unsigned height, unsigned format, unsigned bytesperline)
{
  device_t *dev = buf_list->dev;

  buf_list->v4l2 = calloc(1, sizeof(buffer_list_v4l2_t));

  struct v4l2_capability v4l2_cap;
  E_XIOCTL(dev, dev->v4l2->dev_fd, VIDIOC_QUERYCAP, &v4l2_cap, "Can't query device capabilities");

  if (buf_list->do_capture) {
     if (v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
      buf_list->v4l2->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    } else if (v4l2_cap.capabilities & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE)) {
      buf_list->v4l2->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      buf_list->v4l2->do_mplanes = true;
    } else {
      E_LOG_ERROR(dev, "Video capture is not supported by device: %08x", v4l2_cap.capabilities);
    }
  } else {
    if (v4l2_cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) {
      buf_list->v4l2->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    } else if (v4l2_cap.capabilities & (V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE)) {
      buf_list->v4l2->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      buf_list->v4l2->do_mplanes = true;
    } else {
      E_LOG_ERROR(dev, "Video output is not supported by device: %08x", v4l2_cap.capabilities);
    }
  }

  // Add suffix for debug purposes
  if (v4l2_cap.capabilities & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE)) {
#define MPLANE_SUFFIX ":mplane"
    buf_list->name = realloc(buf_list->name, strlen(buf_list->name) + strlen(MPLANE_SUFFIX) + 1);
    strcat(buf_list->name, MPLANE_SUFFIX);
  }

	struct v4l2_format fmt = { 0 };
  fmt.type = buf_list->v4l2->type;

  unsigned orig_width = width;
  unsigned orig_height = height;

  // JPEG is in 16x16 blocks (shrink image to fit) (but adapt to 32x32)
  // And ISP output
  if (strstr(buf_list->name, "JPEG") || strstr(buf_list->name, "H264") || buf_list->do_capture && strstr(buf_list->name, "ISP")) {
    width = shrink_to_block(width, 32);
    height = shrink_to_block(height, 32);
    E_LOG_VERBOSE(buf_list, "Adapting size to 32x32 block: %dx%d vs %dx%d", orig_width, orig_height, width, height);
  }

  if (format == V4L2_PIX_FMT_H264) {
    bytesperline = 0;
  }

  E_LOG_DEBUG(buf_list, "Get current format ...");
  E_XIOCTL(buf_list, buf_list->dev->v4l2->dev_fd, VIDIOC_G_FMT, &fmt, "Can't set format");

  if (buf_list->v4l2->do_mplanes) {
    fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_JPEG;
    if (width)
      fmt.fmt.pix_mp.width = width;
    if (height)
      fmt.fmt.pix_mp.height = height;
    if (format)
      fmt.fmt.pix_mp.pixelformat = format;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    fmt.fmt.pix_mp.num_planes = 1;
    fmt.fmt.pix_mp.plane_fmt[0].bytesperline = bytesperline;
    //fmt.fmt.pix_mp.plane_fmt[0].sizeimage = bytesperline * orig_height;
  } else {
    fmt.fmt.pix.colorspace = V4L2_COLORSPACE_RAW;
    if (width)
      fmt.fmt.pix.width = width;
    if (height)
      fmt.fmt.pix.height = height;
    if (format)
      fmt.fmt.pix.pixelformat = format;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    fmt.fmt.pix.bytesperline = bytesperline;
    //fmt.fmt.pix.sizeimage = bytesperline * orig_height;
  }

  E_LOG_DEBUG(buf_list, "Configuring format (%s)...", fourcc_to_string(format).buf);
  E_XIOCTL(buf_list, buf_list->dev->v4l2->dev_fd, VIDIOC_S_FMT, &fmt, "Can't set format");

  if (buf_list->v4l2->do_mplanes) {
    buf_list->fmt_width = fmt.fmt.pix_mp.width;
    buf_list->fmt_height = fmt.fmt.pix_mp.height;
    buf_list->fmt_format = fmt.fmt.pix_mp.pixelformat;
    buf_list->fmt_bytesperline = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
  } else {
    buf_list->fmt_width = fmt.fmt.pix.width;
    buf_list->fmt_height = fmt.fmt.pix.height;
    buf_list->fmt_format = fmt.fmt.pix.pixelformat;
    buf_list->fmt_bytesperline = fmt.fmt.pix.bytesperline;
  }

  if (bytesperline > 0 && buf_list->fmt_bytesperline != bytesperline) {
		E_LOG_ERROR(buf_list, "Requested bytesperline=%u. Got %u.",
      bytesperline, buf_list->fmt_bytesperline);
  }

  if (buf_list->fmt_width != width || buf_list->fmt_height != height) {
    if (bytesperline) {
      E_LOG_ERROR(buf_list, "Requested resolution=%ux%u is unavailable. Got %ux%u.",
        width, height, buf_list->fmt_width, buf_list->fmt_height);
    } else {
      E_LOG_INFO(buf_list, "Requested resolution=%ux%u is unavailable. Got %ux%u. Accepted",
        width, height, buf_list->fmt_width, buf_list->fmt_height);
    }
  }

	if (format && buf_list->fmt_format != format) {
		E_LOG_ERROR(buf_list, "Could not obtain the requested format=%s; driver gave us %s",
			fourcc_to_string(format).buf,
			fourcc_to_string(buf_list->fmt_format).buf);
	}

  // Some devices require setting pad size via media-controller
  if (buf_list->do_capture) {
    v4l2_device_set_pad_format(dev, width, height, format);
  }

	E_LOG_INFO(
    buf_list,
    "Using: %ux%u/%s, bytesperline=%d",
    buf_list->fmt_width,
    buf_list->fmt_height,
    fourcc_to_string(buf_list->fmt_format).buf,
    buf_list->fmt_bytesperline
  );

  return 0;

error:
  return -1;
}

int v4l2_buffer_list_set_buffers(buffer_list_t *buf_list, int nbufs)
{
	struct v4l2_requestbuffers v4l2_req = {0};
	v4l2_req.count = nbufs;
	v4l2_req.type = buf_list->v4l2->type;
	v4l2_req.memory = buf_list->do_mmap ? V4L2_MEMORY_MMAP : V4L2_MEMORY_DMABUF;

	E_LOG_DEBUG(buf_list, "Requesting %u buffers", v4l2_req.count);

	E_XIOCTL(buf_list, buf_list->dev->v4l2->dev_fd, VIDIOC_REQBUFS, &v4l2_req, "Can't request buffers");
	if (v4l2_req.count < 1) {
		E_LOG_ERROR(buf_list, "Insufficient buffer memory: %u", v4l2_req.count);
	}

	E_LOG_DEBUG(buf_list, "Got %u buffers", v4l2_req.count);
  return v4l2_req.count;

error:
  return -1;
}

int v4l2_buffer_list_set_stream(buffer_list_t *buf_list, bool do_on)
{
	enum v4l2_buf_type type = buf_list->v4l2->type;
  E_XIOCTL(buf_list, buf_list->dev->v4l2->dev_fd, do_on ? VIDIOC_STREAMON : VIDIOC_STREAMOFF, &type, "Cannot set streaming state");

  return 0;
error:
  return -1;
}

void v4l2_buffer_list_close(buffer_list_t *buf_list) {
  free(buf_list->v4l2);
  buf_list->v4l2 = NULL;
}
