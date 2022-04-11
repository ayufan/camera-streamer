#include "libcamera.hh"

int libcamera_buffer_list_open(buffer_list_t *buf_list, unsigned width, unsigned height, unsigned format, unsigned bytesperline, int nbufs)
{
  int got_bufs = 0;

  if (!buf_list->do_capture) {
    E_LOG_INFO(buf_list, "Only capture mode is supported.");
    return -1;
  }

  if (!buf_list->do_mmap) {
    E_LOG_INFO(buf_list, "Only mmap buffers are supported.");
    return -1;
  }

  buf_list->libcamera = new buffer_list_libcamera_t{};
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
    E_LOG_ERROR(buf_list, "Camera configuration invalid");
  }

  if (buf_list->dev->libcamera->camera->configure(buf_list->libcamera->configuration.get()) < 0) {
    E_LOG_ERROR(buf_list, "Failed to configure camera");
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
      E_LOG_ERROR(buf_list, "Can't allocate buffers");
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
    delete buf_list->libcamera;
    buf_list->libcamera = NULL;
  }
}

int libcamera_buffer_list_set_stream(buffer_list_t *buf_list, bool do_on)
{
  if (do_on) {
    if (buf_list->dev->libcamera->camera->start() < 0) {
      E_LOG_ERROR(buf_list, "Failed to start camera.");
    }
  } else {
    if (buf_list->dev->libcamera->camera->stop() < 0) {
      E_LOG_ERROR(buf_list, "Failed to stop camera.");
    }
  }

  return 0;

error:
  return -1;
}
