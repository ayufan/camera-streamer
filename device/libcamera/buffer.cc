#include "libcamera.hh"

int libcamera_buffer_open(buffer_t *buf)
{
  buf->libcamera = new buffer_libcamera_t{};
  buf->libcamera->request = buf->buf_list->dev->libcamera->camera->createRequest();
  if (!buf->libcamera->request) {
    E_LOG_ERROR(buf, "Can't create request");
  }

  for (libcamera::StreamConfiguration &stream_cfg : *buf->buf_list->libcamera->configuration) {
    auto stream = stream_cfg.stream();
    const std::vector<std::unique_ptr<libcamera::FrameBuffer>> &buffers =
        buf->buf_list->libcamera->allocator->buffers(stream);
    auto const &buffer = buffers[buf->index];

    if (buf->libcamera->request->addBuffer(stream, buffer.get()) < 0) {
      E_LOG_ERROR(buf, "Can't set buffer for request");
    }

    for (auto const &plane : buffer->planes()) {
      if (buf->start) {
        E_LOG_ERROR(buf, "Too many planes open.");
      }

      buf->start = mmap(NULL, plane.length, PROT_READ | PROT_WRITE, MAP_SHARED, plane.fd.get(), 0);
      buf->length = plane.length;
      buf->used = 0;
      buf->dma_fd = plane.fd.get();

      if (!buf->start) {
        E_LOG_ERROR(buf, "Failed to mmap DMA buffer");
      }

      E_LOG_DEBUG(buf, "Mapped buffer: start=%p, length=%d, fd=%d",
        buf->start, buf->length, buf->dma_fd);
    }
  }

  return 0;

error:
  return -1;
}

void libcamera_buffer_close(buffer_t *buf)
{
  if (buf->libcamera) {
    delete buf->libcamera;
    buf->libcamera = NULL;
  }
}

int libcamera_buffer_enqueue(buffer_t *buf, const char *who)
{
  return -1;
}

int libcamera_buffer_list_dequeue(buffer_list_t *buf_list, buffer_t **bufp)
{
  return -1;
}

int libcamera_buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue)
{
  return -1;
}
