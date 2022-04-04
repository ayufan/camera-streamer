#include "device.h"
#include "buffer.h"
#include "buffer_list.h"
#include "links.h"

#define N_FDS 50

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

    struct pollfd fd = {link->capture->fd, POLLIN};
    fds[n] = fd;
    buf_lists[n] = link->capture->capture_list;
    links[n] = link;
    n++;

    for (int j = 0; link->outputs[j]; j++) {
      device_t *output = link->outputs[j];

      if (!output || !output->output_list || n >= max_n) {
        return -EINVAL;
      }
      if (!output->output_list->streaming) {
        continue;
      }

      struct pollfd fd = {output->fd, POLLOUT};
      fds[n] = fd;
      buf_lists[n] = output->output_list;
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
  int n = _build_fds(all_links, fds, links, buf_lists, N_FDS);

  if (poll(fds, n, timeout) < 0 && errno != EINTR) {
    return errno;
  }

  for (int i = 0; i < n; i++) {
    buffer_list_t *buf_list = buf_lists[i];
    link_t *link = links[i];

    if (fds[i].revents & POLLIN) {
      E_LOG_DEBUG(buf_list, "POLLIN");
      if (buf = buffer_list_dequeue(buf_list)) {
        for (int j = 0; link->outputs[j]; j++) {
          buffer_list_enqueue(link->outputs[j]->output_list, buf);
        }

        if (link->on_buffer) {
          link->on_buffer(buf);
        }

        buffer_consumed(buf);
      }
    }
    if (fds[i].revents & POLLOUT) {
      E_LOG_DEBUG(buf_list, "POLLOUT");
      buffer_list_dequeue(buf_list);
    }
  }
  return 0;
}

int links_stream(link_t *all_links, bool do_stream)
{
  for (int i = 0; all_links[i].capture; i++) {
    if (device_stream(all_links[i].capture, true) < 0) {
      E_LOG_ERROR(all_links[i].capture, "Failed to start streaming");
    }

    for (int j = 0; all_links[i].outputs[j]; j++) {
      if (device_stream(all_links[i].outputs[j], true) < 0) {
        E_LOG_ERROR(all_links[i].outputs[j], "Failed to start streaming");
      }
    }
  }

  return 0;

error:
  return -1;
}

int links_open_buffer_list_from(device_t *dev, bool do_capture, buffer_list_t *parent_buffer, unsigned format)
{
  if (!parent_buffer) {
    return -1;
  }

  return device_open_buffer_list(
    dev,
    do_capture,
    parent_buffer->fmt_width,
    parent_buffer->fmt_height,
    format ? format : parent_buffer->fmt_format,
    parent_buffer->nbufs
  );
}

int links_init(link_t *all_links)
{
  // create all outputs (sinks)
  for (int i = 0; all_links[i].capture; i++) {
    link_t *link = &all_links[i];

    if (!link->capture) {
      E_LOG_ERROR(NULL, "Missing link capture.");
    }

    if (link->capture_format) {
      int ret = links_open_buffer_list_from(
        link->capture,
        true,
        link->capture->upstream_device ? link->capture->upstream_device->output_list : link->capture->output_list,
        link->capture_format
      );

      if (ret < 0) {
        E_LOG_ERROR(link->capture, "Failed to create capture_list.");
      }
    }

    if (!link->capture->capture_list) {
      E_LOG_ERROR(link->capture, "Missing capture device.");
    }

    for (int j = 0; link->outputs[j]; j++) {
      device_t *output = link->outputs[j];

      int ret = links_open_buffer_list_from(output, false, link->capture->capture_list, 0);
      if (ret < 0) {
        E_LOG_ERROR(output, "Failed to create output_list.");
      }
    }
  }

  return 0;

error:
  return -1;
}

int links_loop(link_t *all_links, bool *running)
{
  if (links_stream(all_links, true) < 0) {
    return -1;
  }

  *running = true;

  while(*running) {
    if (links_step(all_links, 1000) < 0) {
      links_stream(all_links, false);
      return -1;
    }
  }

  links_stream(all_links, false);
  return 0;
}