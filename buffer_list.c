#include "buffer.h"
#include "buffer_list.h"
#include "device.h"

buffer_list_t *buffer_list_open(const char *name, struct device_s *dev, unsigned type, bool do_mmap)
{
  buffer_list_t *buf_list = calloc(1, sizeof(buffer_list_t));

  buf_list->device = dev;
  buf_list->name = strdup(name);
  buf_list->type = type;

  switch(type) {
  case V4L2_BUF_TYPE_VIDEO_OUTPUT:
    buf_list->do_mmap = do_mmap;
    break;

  case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
    buf_list->do_mmap = do_mmap;
    buf_list->do_mplanes = true;
    break;

  case V4L2_BUF_TYPE_VIDEO_CAPTURE:
    buf_list->do_dma = do_mmap;
    buf_list->do_mmap = do_mmap;
    buf_list->do_capture = true;
    break;

  case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
    buf_list->do_dma = do_mmap;
    buf_list->do_mmap = do_mmap;
    buf_list->do_mplanes = true;
    buf_list->do_capture = true;
    break;

  default:
    E_LOG_PERROR(buf_list, "Unknown type=%d", type);
    goto error;
  }

  return buf_list;

error:
  buffer_list_close(buf_list);
  return NULL;
}

void buffer_list_close(buffer_list_t *buf_list)
{
  if (!buf_list) {
    return;
  }

  if (buf_list->bufs) {
    for (int i = 0; i < buf_list->nbufs; i++) {
      buffer_close(buf_list->bufs[i]);
    }
    free(buf_list->bufs);
    buf_list->bufs = NULL;
    buf_list->nbufs = 0;
  }

  free(buf_list->name);
  free(buf_list);
}

int buffer_list_set_format(buffer_list_t *buf_list, unsigned width, unsigned height, unsigned format)
{
	struct v4l2_format *fmt = &buf_list->v4l2_format;

  fmt->type = buf_list->type;

  if (buf_list->do_mplanes) {
    fmt->fmt.pix_mp.colorspace = V4L2_COLORSPACE_JPEG;
    fmt->fmt.pix_mp.width = width;
    fmt->fmt.pix_mp.height = height;
    fmt->fmt.pix_mp.pixelformat = format;
    fmt->fmt.pix_mp.field = V4L2_FIELD_ANY;
    fmt->fmt.pix_mp.num_planes = 1;
    //fmt->fmt.pix_mp.plane_fmt[0].bytesperline = fourcc_to_stride(width, format);
  } else {
    fmt->fmt.pix.colorspace = V4L2_COLORSPACE_RAW;
    fmt->fmt.pix.width = width;
    fmt->fmt.pix.height = height;
    fmt->fmt.pix.pixelformat = format;
    fmt->fmt.pix.field = V4L2_FIELD_ANY;
    fmt->fmt.pix.bytesperline = fourcc_to_stride(width, format);
  }

  E_LOG_DEBUG(buf_list, "Configuring format ...");
  E_XIOCTL(buf_list, buf_list->device->fd, VIDIOC_S_FMT, fmt, "Can't set format");

  if (fmt->fmt.pix.width != width || fmt->fmt.pix.height < height) {
		E_LOG_ERROR(buf_list, "Requested resolution=%ux%u is unavailable. Got %ux%u.",
      width, height, fmt->fmt.pix.width, fmt->fmt.pix.height);
  }

	if (fmt->fmt.pix.pixelformat != format) {
		E_LOG_ERROR(buf_list, "Could not obtain the requested format=%s; driver gave us %s",
			fourcc_to_string(format).buf,
			fourcc_to_string(fmt->fmt.pix.pixelformat).buf);
	}

	E_LOG_INFO(buf_list, "Using: %ux%u/%s",
    fmt->fmt.pix.width, fmt->fmt.pix.height, fourcc_to_string(fmt->fmt.pix.pixelformat).buf);


  buf_list->fmt_width = fmt->fmt.pix.width;
  buf_list->fmt_height = fmt->fmt.pix.height;
  buf_list->fmt_format = fmt->fmt.pix.pixelformat;

  return 0;

error:
  return -1;
}

int buffer_list_request(buffer_list_t *buf_list, int nbufs)
{
	struct v4l2_requestbuffers v4l2_req = {0};
	v4l2_req.count = nbufs;
	v4l2_req.type = buf_list->type;
	v4l2_req.memory = buf_list->do_mmap ? V4L2_MEMORY_MMAP : V4L2_MEMORY_DMABUF;

	E_LOG_DEBUG(buf_list, "Requesting %u buffers", v4l2_req.count);

	E_XIOCTL(buf_list, buf_list->device->fd, VIDIOC_REQBUFS, &v4l2_req, "Can't request buffers");
	if (v4l2_req.count < 1) {
		E_LOG_ERROR(buf_list, "Insufficient buffer memory: %u", v4l2_req.count);
	}

	E_LOG_DEBUG(buf_list, "Got %u buffers", v4l2_req.count);

  buf_list->bufs = calloc(v4l2_req.count, sizeof(buffer_t*));
  buf_list->nbufs = v4l2_req.count;

  for (unsigned i = 0; i < buf_list->nbufs; i++) {
    char name[64];
    sprintf(name, "%s:buf%d", buf_list->name, i);
    buffer_t *buf = buffer_open(name, buf_list, i);
    if (!buf) {
		  E_LOG_ERROR(buf_list, "Cannot open buffer: %u", i);
      goto error;
    }
    buf_list->bufs[i] = buf;
  }

	E_LOG_DEBUG(buf_list, "Opened %u buffers", buf_list->nbufs);
  return 0;

error:
  return -1;
}

int buffer_list_stream(buffer_list_t *buf_list, bool do_on)
{
  if (!buf_list) {
    return -1;
  }

  if (buf_list->streaming == do_on) {
    return 0;
  }

	enum v4l2_buf_type type = buf_list->type;
  
  E_XIOCTL(buf_list, buf_list->device->fd, do_on ? VIDIOC_STREAMON : VIDIOC_STREAMOFF, &type, "Cannot set streaming state");
  buf_list->streaming = do_on;

  for (int i = 0; i < buf_list->nbufs; i++) {
    buffer_t *buf = buf_list->bufs[i];

    // dequeue buffers (when stream off)
    // if (buf->enqueued) {
    //   if (buf->mmap_source) {
    //     buf->mmap_source->used = 0;
    //     buffer_consumed(buf->mmap_source);
    //     buf->mmap_source = NULL;
    //   }

    //   buf->enqueued = false;
    //   buf->mmap_reflinks = 1;
    // }

    // re-enqueue buffers on stream start
    if (buf_list->streaming && buf_list->do_capture && !buf->enqueued && buf->mmap_reflinks == 1) {
      buffer_consumed(buf);
    }
  }

  E_LOG_DEBUG(buf_list, "Streaming %s...", do_on ? "started" : "stopped");
  return 0;

error:
  return -1;
}
