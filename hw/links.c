#include "hw/device.h"
#include "hw/buffer.h"
#include "hw/buffer_list.h"
#include "hw/links.h"

#define N_FDS 50

void _update_paused(link_t *all_links)
{
  int n = 0;

  for (n = 0; all_links[n].capture; n++);

  for (int i = n; i-- > 0; ) {
    link_t *link = &all_links[i];

    if (!link->capture->capture_list->streaming) {
      continue;
    }

    bool paused = true;

    if (link->callbacks.check_streaming && link->callbacks.check_streaming()) {
      paused = false;
    }

    for (int j = 0; link->sinks[j]; j++) {
      buffer_list_t *sink = link->sinks[j];

      if (!sink->streaming) {
        continue;
      }
      if (sink->device->paused) {
        continue;
      }

      int count_enqueued = buffer_list_count_enqueued(sink);
      if (count_enqueued < sink->nbufs) {
        paused = false;
      }
    }

    link->capture->paused = paused;

    if (link->capture->output_device) {
      link->capture->output_device->paused = paused;
    }
  }
}

int _build_fds(link_t *all_links, struct pollfd *fds, link_t **links, buffer_list_t **buf_lists, int max_n)
{
  int n = 0;

  for (int i = 0; all_links[i].capture; i++) {
    link_t *link = &all_links[i];

    if (!link->capture || !link->capture->capture_list || n >= max_n) {
      return -EINVAL;
    }
    if (!link->capture->capture_list->do_mmap) {
      continue;
    }
    if (!link->capture->capture_list->streaming) {
      continue;
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
      if (count_enqueued == 0) {
        continue;
      }

      struct pollfd fd = {sink->device->fd, POLLOUT};
      fds[n] = fd;
      buf_lists[n] = sink;
      links[n] = link;
      n++;
    }

    if (!link->capture->paused) {
      struct pollfd fd = {link->capture->fd, POLLIN};
      fds[n] = fd;
      buf_lists[n] = link->capture->capture_list;
      links[n] = link;
      n++;
    }
  }

  return n;
}

int links_step(link_t *all_links, int timeout)
{
  struct pollfd fds[N_FDS] = {0};
  link_t *links[N_FDS];
  buffer_list_t *buf_lists[N_FDS];
  buffer_t *buf;

  _update_paused(all_links);

  int n = _build_fds(all_links, fds, links, buf_lists, N_FDS);
  int ret = poll(fds, n, timeout);

  if (ret < 0 && errno != EINTR) {
    return errno;
  }

  for (int i = 0; i < n; i++) {
    buffer_list_t *buf_list = buf_lists[i];
    link_t *link = links[i];

    E_LOG_DEBUG(buf_list, "pool i=%d revents=%08x streaming=%d enqueued=%d/%d paused=%d", i, fds[i].revents, buf_list->streaming,
      buffer_list_count_enqueued(buf_list), buf_list->nbufs, link->capture->paused);

    if (fds[i].revents & POLLIN) {
      E_LOG_DEBUG(buf_list, "POLLIN");
      if (buf = buffer_list_dequeue(buf_list)) {
        for (int j = 0; link->sinks[j]; j++) {
          if (link->sinks[j]->device->paused) {
            continue;
          }
          buffer_list_enqueue(link->sinks[j], buf);
        }

        if (link->callbacks.on_buffer) {
          link->callbacks.on_buffer(buf);
        }

        buffer_consumed(buf);
      }
    }

    // Dequeue buffers that were processed
    if (fds[i].revents & POLLOUT) {
      E_LOG_DEBUG(buf_list, "POLLOUT");
      buffer_list_dequeue(buf_list);
    }

    if (fds[i].revents & POLLERR) {
      E_LOG_INFO(buf_list, "Got an error");
      return -1;
    }

    if (fds[i].revents & ~(POLLIN|POLLOUT|POLLERR)) {
      E_LOG_DEBUG(buf_list, "POLL%08x", fds[i].revents);
    }
  }
  return 0;
}

int links_stream(link_t *all_links, bool do_stream)
{
  int n;

  for (n = 0; all_links[n].capture; n++);

  for (int i = n; --i >= 0; ) {
    bool streaming = true;
    link_t *link = &all_links[i];

    if (buffer_list_stream(link->capture->capture_list, streaming) < 0) {
      E_LOG_ERROR(link->capture, "Failed to start streaming");
    }

    if (device_set_stream(link->capture, streaming) < 0) {
      E_LOG_ERROR(link->capture, "Failed to start streaming");
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