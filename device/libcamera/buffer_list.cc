#ifdef USE_LIBCAMERA
#include "libcamera.hh"

int libcamera_buffer_list_open(buffer_list_t *buf_list, unsigned width, unsigned height, unsigned format, unsigned bytesperline, int nbufs)
{
  int got_bufs = 0;

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

  buf_list->libcamera->configuration = buf_list->dev->libcamera->camera->generateConfiguration(
    { libcamera::StreamRole::Viewfinder });

  auto &configuration = buf_list->libcamera->configuration->at(0);
  configuration.size = libcamera::Size(width, height);
  configuration.pixelFormat = libcamera::PixelFormat(format);
  if (bytesperline > 0) {
    configuration.stride = bytesperline;
  }
  if (nbufs > 0) {
    configuration.bufferCount = nbufs;
  }
  if (buf_list->libcamera->configuration->validate() == libcamera::CameraConfiguration::Invalid) {
    LOG_ERROR(buf_list, "Camera configuration invalid");
  }

  if (buf_list->dev->libcamera->camera->configure(buf_list->libcamera->configuration.get()) < 0) {
    LOG_ERROR(buf_list, "Failed to configure camera");
  }

  buf_list->fmt_width = configuration.size.width;
  buf_list->fmt_height = configuration.size.height;
  buf_list->fmt_format = configuration.pixelFormat.fourcc();
  buf_list->fmt_bytesperline = configuration.stride;

  buf_list->libcamera->allocator = std::make_shared<libcamera::FrameBufferAllocator>(
    buf_list->dev->libcamera->camera);

  got_bufs = configuration.bufferCount;

  for (libcamera::StreamConfiguration &stream_config : *buf_list->libcamera->configuration) {
    if (buf_list->libcamera->allocator->allocate(stream_config.stream()) < 0) {
      LOG_ERROR(buf_list, "Can't allocate buffers");
    }

    int allocated = buf_list->libcamera->allocator->buffers(
      stream_config.stream()).size();
    got_bufs = std::min(got_bufs, allocated);
  }

  return got_bufs;

error:
  return -1;
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

    if (buf_list->dev->libcamera->camera->start() < 0) {
      LOG_ERROR(buf_list, "Failed to start camera.");
    }
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
