#ifdef USE_LIBCAMERA
#include "libcamera.hh"

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

    for (auto const &plane : buffer->planes()) {
      if (buf->start) {
        LOG_ERROR(buf, "Too many planes open.");
      }

      buf->start = mmap(NULL, plane.length, PROT_READ | PROT_WRITE, MAP_SHARED, plane.fd.get(), 0);
      buf->length = plane.length;
      buf->used = 0;
      buf->dma_fd = plane.fd.get();

      if (!buf->start) {
        LOG_ERROR(buf, "Failed to mmap DMA buffer");
      }

      LOG_DEBUG(buf, "Mapped buffer: start=%p, length=%d, fd=%d",
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
  auto &request = buf->libcamera->request;
  auto const &camera = buf->buf_list->dev->libcamera->camera;

  request->reuse(libcamera::Request::ReuseBuffers);

  // TODO: assign timestamps
  // for (auto const &pair : request->buffers()) {
  //   v4l2_buf.timestamp.tv_sec = buf->captured_time_us / (1000LL * 1000LL);
  //   v4l2_buf.timestamp.tv_usec = buf->captured_time_us % (1000LL * 1000LL);
  // }

  if (buf->buf_list->dev->libcamera->camera->queueRequest(buf->libcamera->request.get()) < 0) {
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

  if (index >= buf_list->nbufs) {
    LOG_INFO(buf_list, "Received invalid index from `read`: %d >= %d", index, buf_list->nbufs);
    return -1;
  }

  *bufp = buf_list->bufs[index];

  // TODO: fix timestamps
  // buf->v4l2->flags = v4l2_buf.flags;
  // buf->flags.is_keyframe = (v4l2_buf.flags & V4L2_BUF_FLAG_KEYFRAME) != 0;
  // buf->captured_time_us = get_time_us(CLOCK_FROM_PARAMS, NULL, &v4l2_buf.timestamp, 0);
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
