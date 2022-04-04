#include "device.h"
#include "buffer.h"
#include "buffer_list.h"
#include "links.h"

#define N_FDS 50

int _build_fds(link_t *all_links, struct pollfd *fds, link_t **links, buffer_list_t **buf_lists, int max_n)
{
  int n;

  for (int i = 0; all_links[i].capture; i++) {
    link_t *link = &all_links[i];

    if (!link->capture || !link->capture->capture_list || n >= max_n) {
      return -EINVAL;
    }
    if (!link->capture->capture_list->do_mmap) {
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
      if (output->output_list->do_mmap) {
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

int handle_links(link_t *all_links, int timeout)
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
      if (buf = buffer_list_dequeue(buf_list, true)) {
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
      if (buf = buffer_list_dequeue(buf_list, false)) {
        buffer_consumed(buf);
      }
    }
  }
  return 0;
}
