#include "hw/device.h"
#include "hw/buffer.h"
#include "hw/buffer_list.h"
#include "hw/links.h"

#define N_FDS 50

int _build_fds(link_t *all_links, struct pollfd *fds, link_t **links, buffer_list_t **buf_lists, int max_n)
{
  int n = 0, nlinks = 0;

  for (nlinks = 0; all_links[nlinks].source; nlinks++);

  // This traverses in reverse order as it requires to first fix outputs
  // and go back into captures

  for (int i = nlinks; i-- > 0; ) {
    link_t *link = &all_links[i];
    buffer_list_t *source = link->source;

    if (n >= max_n) {
      return -EINVAL;
    }
    if (!source->streaming) {
      continue;
    }

    bool paused = true;

    if (link->callbacks.check_streaming && link->callbacks.check_streaming()) {
      paused = false;
    }

    for (int j = 0; link->sinks[j]; j++) {
      buffer_list_t *sink = link->sinks[j];

      if (n >= max_n) {
        return -EINVAL;
      }
      if (!sink->streaming) {
        continue;
      }

      int count_enqueued = buffer_list_count_enqueued(sink);

      // Can something be dequeued?
      fds[n].fd = sink->device->fd;
      fds[n].events = POLLHUP;
      if (count_enqueued > 0)
        fds[n].events |= POLLOUT;
      buf_lists[n] = source;
      links[n] = link;
      n++;

      // Can this chain pauses
      if (!sink->device->paused && count_enqueued < sink->nbufs) {
        paused = false;
      }
    }

    source->device->paused = paused;

    if (source->device->output_device) {
      source->device->output_device->paused = paused;
    }

    fds[n].fd = source->device->fd;
    fds[n].events = POLLHUP;
    if (!source->device->paused)
      fds[n].events |= POLLIN;
    buf_lists[n] = source;
    links[n] = link;
    n++;
  }

  return n;
}

int links_enqueue_from_source(buffer_list_t *buf_list, link_t *link)
{
  if (!link) {
    E_LOG_ERROR(buf_list, "Missing link for source");
  }

  buffer_t *buf = buffer_list_dequeue(buf_list);
  if (!buf) {
    E_LOG_ERROR(buf_list, "No buffer dequeued from source?");
  }

  for (int j = 0; link->sinks[j]; j++) {
    if (link->sinks[j]->device->paused) {
      continue;
    }
    buffer_list_enqueue(link->sinks[j], buf);
  }

  if (link->callbacks.on_buffer) {
    link->callbacks.on_buffer(buf);
  }

  buffer_consumed(buf, "link-from-source");
  return 0;

error:
  return -1;
}

int links_dequeue_from_sink(buffer_list_t *buf_list) {
  buffer_t *buf = buffer_list_dequeue(buf_list);
  if (!buf) {
    E_LOG_ERROR(buf, "No buffer dequeued from sink?");
  }

  return 0;

error:
  return -1;
}

int links_step(link_t *all_links, int timeout)
{
  struct pollfd fds[N_FDS] = {0};
  link_t *links[N_FDS];
  buffer_list_t *buf_lists[N_FDS];
  buffer_t *buf;

  int n = _build_fds(all_links, fds, links, buf_lists, N_FDS);
  int ret = poll(fds, n, timeout);

  if (ret < 0 && errno != EINTR) {
    return errno;
  }

  for (int i = 0; i < n; i++) {
    buffer_list_t *buf_list = buf_lists[i];
    link_t *link = links[i];

    E_LOG_DEBUG(buf_list, "pool event=%s%s%s%s/%08x streaming=%d enqueued=%d/%d",
      !fds[i].revents ? "NONE" : "",
      fds[i].revents & POLLIN ? "IN" : "",
      fds[i].revents & POLLOUT ? "OUT" : "",
      fds[i].revents & POLLERR ? "ERR" : "",
      fds[i].revents,
      buf_list->streaming,
      buffer_list_count_enqueued(buf_list),
      buf_list->nbufs);

    if (fds[i].revents & POLLIN) {
      if (links_enqueue_from_source(buf_list, link) < 0) {
        return -1;
      }
    }

    // Dequeue buffers that were processed
    if (fds[i].revents & POLLOUT) {
      if (links_dequeue_from_sink(buf_list) < 0) {
        return -1;
      }
    }

    if (fds[i].revents & POLLHUP) {
      E_LOG_INFO(buf_list, "Device disconnected.");
      return -1;
    }

    if (fds[i].revents & POLLERR) {
      E_LOG_INFO(buf_list, "Got an error");
      return -1;
    }
  }
  return 0;
}

int links_stream(link_t *all_links, bool do_stream)
{
  for (int i = 0; all_links[i].source; i++) {
    bool streaming = true;
    link_t *link = &all_links[i];

    if (buffer_list_stream(link->source, streaming) < 0) {
      E_LOG_ERROR(link->source, "Failed to start streaming");
    }

    for (int j = 0; link->sinks[j]; j++) {
      if (buffer_list_stream(link->sinks[j], streaming) < 0) {
        E_LOG_ERROR(link->sinks[j], "Failed to start streaming");
      }
    }
  }

  return 0;

error:
  return -1;
}

int links_loop(link_t *all_links, bool *running)
{
  *running = true;

  if (links_stream(all_links, true) < 0) {
    return -1;
  }

  while(*running) {
    if (links_step(all_links, 1000) < 0) {
      links_stream(all_links, false);
      return -1;
    }
  }

  links_stream(all_links, false);
  return 0;
}