#ifdef USE_LIBCAMERA
#include "libcamera.hh"

#include <inttypes.h>

int libcamera_buffer_open(buffer_t *buf)
{
  buf->libcamera = new buffer_libcamera_t{};
  buf->libcamera->request = buf->buf_list->dev->libcamera->camera->createRequest(buf->index);
  if (!buf->libcamera->request) {
    LOG_ERROR(buf, "Can't create request");
  }

  for (libcamera::StreamConfiguration &stream_cfg : *buf->buf_list->libcamera->configuration) {
    auto stream = stream_cfg.stream();
    const std::vector<std::unique_ptr<libcamera::FrameBuffer>> &buffers =
        buf->buf_list->libcamera->allocator->buffers(stream);
    auto const &buffer = buffers[buf->index];

    if (buf->libcamera->request->addBuffer(stream, buffer.get()) < 0) {
      LOG_ERROR(buf, "Can't set buffer for request");
    }
    if (buf->start) {
      LOG_ERROR(buf, "Too many streams.");
    }

    if (buffer->planes().empty()) {
      LOG_ERROR(buf, "No planes allocated");
    }

    uint64_t offset = buffer->planes()[0].offset;
    uint64_t length = 0;
    libcamera::SharedFD dma_fd = buffer->planes()[0].fd;

    // Require that planes are continuous
    for (auto const &plane : buffer->planes()) {
      if (plane.fd != dma_fd) {
        LOG_ERROR(buf, "Plane does not share FD: fd=%d, expected=%d", plane.fd.get(), dma_fd.get());
      }

      if (offset + length != plane.offset) {
        LOG_ERROR(buf, "Plane is not continuous: offset=%u, expected=%" PRIu64, plane.offset, offset + length);
      }

      length += plane.length;
    }

    buf->start = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, dma_fd.get(), offset);
    if (!buf->start || buf->start == MAP_FAILED) {
      LOG_ERROR(buf, "Failed to mmap DMA buffer");
    }

    buf->dma_fd = dma_fd.get();
    buf->length = length;

    LOG_DEBUG(buf, "Mapped buffer: start=%p, length=%zu, fd=%d, planes=%zu",
      buf->start, buf->length, buf->dma_fd, buffer->planes().size());
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
  auto &request = buf->libcamera->request;
  auto const &camera = buf->buf_list->dev->libcamera->camera;

  request->reuse(libcamera::Request::ReuseBuffers);

  if (camera->queueRequest(buf->libcamera->request.get()) < 0) {
    LOG_ERROR(buf, "Can't queue buffer.");
  }
  return 0;

error:
  return -1;
}

void buffer_list_libcamera_t::libcamera_buffer_list_dequeued(libcamera::Request *request)
{
  if (request->status() == libcamera::Request::RequestComplete) {
    unsigned index = request->cookie();
    if (write(buf_list->libcamera->fds[1], &index, sizeof(index)) == sizeof(index)) {
      return;
    }
  }

  // put back into queue, as it failed
  buf_list->dev->libcamera->camera->queueRequest(request);
}

int libcamera_buffer_list_dequeue(buffer_list_t *buf_list, buffer_t **bufp)
{
  unsigned index = 0;
  int n = read(buf_list->libcamera->fds[0], &index, sizeof(index));
  if (n != sizeof(index)) {
    LOG_INFO(buf_list, "Received invalid result from `read`: %d", n);
    return -1;
  }

  if (index >= (unsigned)buf_list->nbufs) {
    LOG_INFO(buf_list, "Received invalid index from `read`: %d >= %d", index, buf_list->nbufs);
    return -1;
  }

  *bufp = buf_list->bufs[index];
  return 0;
}

int libcamera_buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue)
{
  int count_enqueued = buffer_list_count_enqueued(buf_list);
  pollfd->fd = buf_list->libcamera->fds[0]; // write end
  pollfd->events = POLLHUP;
  if (can_dequeue && count_enqueued > 0) {
    pollfd->events |= POLLIN;
  }
  pollfd->revents = 0;
  return 0;
}
#endif // USE_LIBCAMERA
