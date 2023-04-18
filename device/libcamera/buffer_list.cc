#ifdef USE_LIBCAMERA
#include "libcamera.hh"

struct libcamera_format_s {
  unsigned fourcc;
  libcamera::PixelFormat pixelFormat;
};

static libcamera_format_s libcamera_formats[] = {
  { V4L2_PIX_FMT_RGB24, libcamera::formats::RGB888 },
  { V4L2_PIX_FMT_BGR24, libcamera::formats::BGR888 },
  { 0 },
};

libcamera::PixelFormat libcamera_from_fourcc(unsigned fourcc)
{
  for (int i = 0; libcamera_formats[i].fourcc; i++) {
    if (libcamera_formats[i].fourcc == fourcc) {
      return libcamera_formats[i].pixelFormat;
    }
  }

  return libcamera::PixelFormat(fourcc);
}

unsigned libcamera_to_fourcc(libcamera::PixelFormat pixelFormat)
{
  for (int i = 0; libcamera_formats[i].fourcc; i++) {
    if (libcamera_formats[i].pixelFormat == pixelFormat) {
      return libcamera_formats[i].fourcc;
    }
  }

  return pixelFormat.fourcc();
}

int libcamera_buffer_list_open(buffer_list_t *buf_list)
{
  if (!buf_list->do_capture) {
    LOG_INFO(buf_list, "Only capture mode is supported.");
    return -1;
  }

  if (!buf_list->do_mmap) {
    LOG_INFO(buf_list, "Only mmap buffers are supported.");
    return -1;
  }

  buf_list->libcamera = new buffer_list_libcamera_t{};
  buf_list->libcamera->buf_list = buf_list;
  buf_list->libcamera->fds[0] = -1;
  buf_list->libcamera->fds[1] = -1;

  if (pipe2(buf_list->libcamera->fds, O_DIRECT|O_CLOEXEC) < 0) {
    LOG_INFO(buf_list, "Cannot open `pipe2`.");
    return -1;
  }

  auto &configurations = buf_list->dev->libcamera->configuration;

  // add new configuration based on a buffer
  {
    libcamera::StreamRole role = libcamera::StreamRole::StillCapture;

    switch(buf_list->fmt.type) {
    case BUFFER_TYPE_RAW:
      role = libcamera::StreamRole::Raw;
      break;

    case BUFFER_TYPE_VIDEO:
      role = libcamera::StreamRole::VideoRecording;
      break;

    default:
      role = libcamera::StreamRole::StillCapture;
      break;
    }

    auto newConfigurations = buf_list->dev->libcamera->camera->generateConfiguration({ role });
    configurations->addConfiguration(newConfigurations->at(0));
  }

  if (buf_list->index >= (int)configurations->size()) {
    LOG_INFO(buf_list, "Not enough configurations.");
    return -1;
  }

  auto &configuration = configurations->at(buf_list->index);
  configuration.size = libcamera::Size(buf_list->fmt.width, buf_list->fmt.height);
  if (buf_list->fmt.format) {
    configuration.pixelFormat = libcamera_from_fourcc(buf_list->fmt.format);
  }
  if (buf_list->fmt.bytesperline > 0) {
    configuration.stride = buf_list->fmt.bytesperline;
  }
  if (buf_list->fmt.nbufs > 0) {
    configuration.bufferCount = buf_list->fmt.nbufs;
  }
  if (configurations->validate() == libcamera::CameraConfiguration::Invalid) {
    LOG_ERROR(buf_list, "Camera configuration invalid");
  }
  if (buf_list->dev->libcamera->vflip) {
    configurations->transform |= libcamera::Transform::VFlip;
  }
  if (buf_list->dev->libcamera->hflip) {
    configurations->transform |= libcamera::Transform::HFlip;
  }
  if (!!(configurations->transform & libcamera::Transform::Transpose)) {
    LOG_ERROR(buf_list, "Transformation requiring transpose not supported");
  }

  if (buf_list->dev->libcamera->camera->configure(configurations.get()) < 0) {
    LOG_ERROR(buf_list, "Failed to configure camera");
  }

  buf_list->fmt.width = configuration.size.width;
  buf_list->fmt.height = configuration.size.height;
  buf_list->fmt.format = libcamera_to_fourcc(configuration.pixelFormat);
  buf_list->fmt.bytesperline = configuration.stride;
  return 0;

error:
  return -1;
}

int libcamera_buffer_list_alloc_buffers(buffer_list_t *buf_list)
{
  auto &configurations = buf_list->dev->libcamera->configuration;
  auto &configuration = configurations->at(buf_list->index);

  if (buf_list->dev->libcamera->allocator->allocate(configuration.stream()) < 0) {
    LOG_ERROR(buf_list, "Can't allocate buffers");
  }

  {
    int allocated = buf_list->dev->libcamera->allocator->buffers(
      configuration.stream()).size();
    return std::min<int>(buf_list->fmt.nbufs, allocated);
  }

error:
  return -1;
}

void libcamera_buffer_list_free_buffers(buffer_list_t *buf_list)
{
  auto &configurations = buf_list->dev->libcamera->configuration;
  auto &configuration = configurations->at(buf_list->index);
  buf_list->dev->libcamera->allocator->free(configuration.stream());
}

void libcamera_buffer_list_close(buffer_list_t *buf_list)
{
  if (buf_list->libcamera) {
    close(buf_list->libcamera->fds[0]);
    close(buf_list->libcamera->fds[1]);

    delete buf_list->libcamera;
    buf_list->libcamera = NULL;
  }
}

int libcamera_buffer_list_set_stream(buffer_list_t *buf_list, bool do_on)
{
  if (do_on) {
    buf_list->dev->libcamera->camera->requestCompleted.connect(
      buf_list->libcamera, &buffer_list_libcamera_t::libcamera_buffer_list_dequeued);

    if (buf_list->dev->libcamera->camera->start(&buf_list->dev->libcamera->controls) < 0) {
      LOG_ERROR(buf_list, "Failed to start camera.");
    }

    buf_list->dev->libcamera->controls.clear();
  } else {
    buf_list->dev->libcamera->camera->requestCompleted.disconnect(
      buf_list->libcamera, &buffer_list_libcamera_t::libcamera_buffer_list_dequeued);

    if (buf_list->dev->libcamera->camera->stop() < 0) {
      LOG_ERROR(buf_list, "Failed to stop camera.");
    }
  }

  return 0;

error:
  return -1;
}
#endif // USE_LIBCAMERA
