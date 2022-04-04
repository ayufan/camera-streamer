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

    bool can_consume = true;

    for (int j = 0; link->outputs[j]; j++) {
      device_t *output = link->outputs[j];

      if (!output || !output->output_list || n >= max_n) {
        return -EINVAL;
      }
      if (!output->output_list->streaming) {
        continue;
      }

      int count_enqueued = buffer_list_count_enqueued(output->output_list);

      if (count_enqueued == output->output_list->nbufs) {
        E_LOG_DEBUG(link->capture->capture_list, "Cannot consume due to %s using %d of %d",
          output->output_list->name, count_enqueued, output->output_list->nbufs);
        can_consume = false;
      }

      // Can something be dequeued?
      if (count_enqueued == 0) {
        continue;
      }

      struct pollfd fd = {output->fd, POLLOUT};
      fds[n] = fd;
      buf_lists[n] = output->output_list;
      links[n] = link;
      n++;
    }

    if (can_consume) {
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
  int n = _build_fds(all_links, fds, links, buf_lists, N_FDS);
  int ret = poll(fds, n, timeout);

  if (ret < 0 && errno != EINTR) {
    return errno;
  }

  printf("links_step n=%d, ret=%d\n", n, ret);

  for (int i = 0; i < n; i++) {
    buffer_list_t *buf_list = buf_lists[i];
    link_t *link = links[i];

    E_LOG_DEBUG(buf_list, "pool i=%d revents=%08x streaming=%d enqueued=%d/%d", i, fds[i].revents, buf_list->streaming,
      buffer_list_count_enqueued(buf_list), buf_list->nbufs);

    if (fds[i].revents & POLLIN) {
      E_LOG_DEBUG(buf_list, "POLLIN");
      if (buf = buffer_list_dequeue(buf_list)) {
        for (int j = 0; link->outputs[j]; j++) {
          buffer_list_enqueue(link->outputs[j]->output_list, buf);
        }

        if (link->callbacks.on_buffer) {
          link->callbacks.on_buffer(buf);
        }

        buffer_consumed(buf);
      }
    }
    if (fds[i].revents & POLLOUT) {
      E_LOG_DEBUG(buf_list, "POLLOUT");
      buffer_list_dequeue(buf_list);
    }
    if (fds[i].revents & POLLERR) {
      E_LOG_DEBUG(buf_list, "POLLERR");
      device_consume_event(buf_list->device);
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
    
    if (link->callbacks.check_streaming) {
      streaming = link->callbacks.check_streaming();
    }

    if (buffer_list_stream(link->capture->capture_list, streaming) < 0) {
      E_LOG_ERROR(link->capture, "Failed to start streaming");
    }

    for (int j = 0; link->outputs[j]; j++) {
      if (buffer_list_stream(link->outputs[j]->output_list, streaming) < 0) {
        E_LOG_ERROR(link->outputs[j], "Failed to start streaming");
      }
    }
  }

  return 0;

error:
  return -1;
}

// int links_open_buffer_list_from(device_t *dev, bool do_capture, buffer_list_t *parent_buffer, unsigned format)
// {
//   if (!parent_buffer) {
//     return -1;
//   }

//   return device_open_buffer_list(
//     dev,
//     do_capture,
//     parent_buffer->fmt_width,
//     parent_buffer->fmt_height,
//     format ? format : parent_buffer->fmt_format,
//     parent_buffer->nbufs
//   );
// }

// int links_init(link_t *all_links)
// {
//   // create all outputs (sinks)
//   for (int i = 0; all_links[i].capture; i++) {
//     link_t *link = &all_links[i];

//     if (!link->capture) {
//       E_LOG_ERROR(NULL, "Missing link capture.");
//     }

//     if (link->capture_format) {
//       int ret = links_open_buffer_list_from(
//         link->capture,
//         true,
//         link->capture->upstream_device ? link->capture->upstream_device->output_list : link->capture->output_list,
//         link->capture_format
//       );

//       if (ret < 0) {
//         E_LOG_ERROR(link->capture, "Failed to create capture_list.");
//       }
//     }

//     if (!link->capture->capture_list) {
//       E_LOG_ERROR(link->capture, "Missing capture device.");
//     }

//     for (int j = 0; link->outputs[j]; j++) {
//       device_t *output = link->outputs[j];

//       int ret = links_open_buffer_list_from(output, false, link->capture->capture_list, 0);
//       if (ret < 0) {
//         E_LOG_ERROR(output, "Failed to create output_list.");
//       }
//     }
//   }

//   return 0;

// error:
//   return -1;
// }

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
    //usleep(100*1000);
  }

  links_stream(all_links, false);
  return 0;
}