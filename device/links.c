#include "device/links.h"
#include "device/device.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/buffer_lock.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"

#define N_FDS 50
#define QUEUE_ON_CAPTURE // seems to provide better latency
// #define LIMIT_CAPTURE_BUFFERS

int _build_fds(link_t *all_links, struct pollfd *fds, link_t **links, buffer_list_t **buf_lists, int max_n, int *max_timeout_ms)
{
  int n = 0, nlinks = 0;

  uint64_t now_us __attribute__((unused)) = get_monotonic_time_us(NULL, NULL);

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

    for (int j = 0; j < link->n_callbacks; j++) {
      if (link->callbacks[j].check_streaming && link->callbacks[j].check_streaming()) {
        paused = false;
      }

      if (link->callbacks[j].buf_lock && buffer_lock_needs_buffer(link->callbacks[j].buf_lock)) {
        paused = false;
      }
    }

    for (int j = 0; link->sinks[j]; j++) {
      buffer_list_t *sink = link->sinks[j];

      if (n >= max_n) {
        return -EINVAL;
      }
      if (!sink->streaming) {
        continue;
      }

      // Can something be dequeued?
      if (buffer_list_pollfd(sink, &fds[n], true) == 0) {
        buf_lists[n] = sink;
        links[n] = NULL;
        n++;
      }

      // Can this chain pauses
      int count_enqueued = buffer_list_count_enqueued(sink);
      if (!sink->dev->paused && count_enqueued < sink->nbufs) {
        paused = false;
      } else if (count_enqueued > 0) {
        paused = false;
      }
    }

    source->dev->paused = paused;

    int count_enqueued = buffer_list_count_enqueued(source);
    bool can_dequeue = count_enqueued > 0;

#ifndef QUEUE_ON_CAPTURE
    if (now_us - source->last_dequeued_us < source->fmt.interval_us) {
      can_dequeue = false;
      *max_timeout_ms = MIN(*max_timeout_ms, (source->last_dequeued_us + source->fmt.interval_us - now_us) / 1000);
    }
#endif

    if (buffer_list_pollfd(source, &fds[n], can_dequeue) == 0) {
      buf_lists[n] = source;
      links[n] = link;
      n++;
    }
  }

  return n;
}

bool links_sink_can_enqueue(buffer_list_t *buf_list)
{
  int current = buffer_list_count_enqueued(buf_list);

  if (buf_list->do_capture) {
    perror("should not happen");
  }

  int capture_max = 0;

  for (int i = 0; i < buf_list->dev->n_capture_list; i++) {
    int capture_count = buffer_list_count_enqueued(buf_list->dev->capture_lists[i]);
    if (capture_max < capture_count)
      capture_max = capture_count;
  }

  // only enqueue on output, if there are already captures (and there's more of them)
  if (capture_max <= current) {
    LOG_DEBUG(buf_list, "Skipping enqueue of output (output_enqueued=%d, capture_enqueued=%d)",
      current, capture_max);
    return false;
  }

  return true;
}

int links_enqueue_from_source(buffer_list_t *buf_list, link_t *link)
{
  if (!link) {
    LOG_ERROR(buf_list, "Missing link for source");
  }

  buffer_t *buf = buffer_list_dequeue(buf_list);
  if (!buf) {
    LOG_ERROR(buf_list, "No buffer dequeued from source?");
  }

  for (int j = 0; j < link->n_callbacks; j++) {
    if (link->callbacks[j].validate_buffer && !link->callbacks[j].validate_buffer(link, buf)) {
      LOG_DEBUG(buf_list, "Buffer rejected by validation");
      return 0;
    }
  }

  for (int j = 0; link->sinks[j]; j++) {
    if (link->sinks[j]->dev->paused) {
      continue;
    }
    if (!links_sink_can_enqueue(link->sinks[j])) {
      continue;
    }
    buffer_list_enqueue(link->sinks[j], buf);
  }

  for (int j = 0; j < link->n_callbacks; j++) {
    if (link->callbacks[j].on_buffer) {
      link->callbacks[j].on_buffer(buf);
    }

    if (link->callbacks[j].buf_lock) {
      buffer_lock_capture(link->callbacks[j].buf_lock, buf);
    }
  }

  return 0;

error:
  return -1;
}

int links_dequeue_from_sink(buffer_list_t *buf_list) {
  buffer_t *buf = buffer_list_dequeue(buf_list);
  if (!buf) {
    LOG_ERROR(buf, "No buffer dequeued from sink?");
  }

  return 0;

error:
  return -1;
}

void print_pollfds(struct pollfd *fds, int n)
{
  if (!getenv("DEBUG_FDS")) {
    return;
  }

  for (int i = 0; i < n; i++) {
    printf("poll(i=%i, fd=%d, events=%08x, revents=%08x)\n", i, fds[i].fd, fds[i].events, fds[i].revents);
  }
  printf("pollfds = %d\n", n);
}

int links_step(link_t *all_links, int timeout_now_ms, int *timeout_next_ms)
{
  struct pollfd fds[N_FDS] = {0};
  link_t *links[N_FDS];
  buffer_list_t *buf_lists[N_FDS];

  int n = _build_fds(all_links, fds, links, buf_lists, N_FDS, &timeout_now_ms);
  print_pollfds(fds, n);
  int ret = poll(fds, n, timeout_now_ms);
  print_pollfds(fds, n);

  uint64_t now_us __attribute__((unused)) = get_monotonic_time_us(NULL, NULL);

  if (ret < 0 && errno != EINTR) {
    return errno;
  }

  for (int i = 0; i < n; i++) {
    buffer_list_t *buf_list = buf_lists[i];
    link_t *link = links[i];

    LOG_DEBUG(buf_list, "pool event=%s%s%s%s%s%08x streaming=%d enqueued=%d/%d paused=%d",
      !fds[i].revents ? "NONE/" : "",
      fds[i].revents & POLLIN ? "IN/" : "",
      fds[i].revents & POLLOUT ? "OUT/" : "",
      fds[i].revents & POLLHUP ? "HUP/" : "",
      fds[i].revents & POLLERR ? "ERR/" : "",
      fds[i].revents,
      buf_list->streaming,
      buffer_list_count_enqueued(buf_list),
      buf_list->nbufs,
      buf_list->dev->paused);

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
      LOG_INFO(buf_list, "Device disconnected.");
      return -1;
    }

    if (fds[i].revents & POLLERR) {
      LOG_INFO(buf_list, "Got an error");
      return -1;
    }

    if (!buf_list->dev->paused && buf_list->do_capture && buf_list->do_mmap) {
      buffer_t *buf;

#ifdef QUEUE_ON_CAPTURE
      if (buf_list->fmt.interval_us > 0 && now_us - buf_list->last_enqueued_us < buf_list->fmt.interval_us) {
        *timeout_next_ms = MIN(*timeout_next_ms, (buf_list->last_enqueued_us + buf_list->fmt.interval_us - now_us) / 1000);

        LOG_DEBUG(buf_list, "skipping dequeue: %.1f / %.1f. enqueued=%d",
          (now_us - buf_list->last_enqueued_us) / 1000.0f,
          buf_list->fmt.interval_us / 1000.0f,
          buffer_list_count_enqueued(buf_list));
        continue;
      } else if (buf_list->fmt.interval_us > 0) {
        LOG_DEBUG(buf_list, "since last: %.1f / %.1f. enqueued=%d",
          (now_us - buf_list->last_enqueued_us) / 1000.0f,
          buf_list->fmt.interval_us / 1000.0f,
          buffer_list_count_enqueued(buf_list));
      }
#else
      // feed capture queue (two buffers)
      int count_enqueued = buffer_list_count_enqueued(buf_list);
      if (count_enqueued > 1)
        continue;
#endif

#ifdef LIMIT_CAPTURE_BUFFERS
      // Do not enqueue more buffers than enqueued by output
      if (buf_list->dev->output_list && buffer_list_count_enqueued(buf_list) >= buffer_list_count_enqueued(buf_list->dev->output_list)) {
        continue;
      }
#endif

      if ((buf = buffer_list_find_slot(buf_list)) != NULL) {
        buffer_consumed(buf, "enqueued");
      }
    }
  }
  return 0;
}

int links_stream(link_t *all_links, bool do_stream)
{
  for (int i = 0; all_links[i].source; i++) {
    bool streaming = true;
    link_t *link = &all_links[i];

    if (buffer_list_set_stream(link->source, streaming) < 0) {
      LOG_ERROR(link->source, "Failed to start streaming");
    }

    for (int j = 0; link->sinks[j]; j++) {
      if (buffer_list_set_stream(link->sinks[j], streaming) < 0) {
        LOG_ERROR(link->sinks[j], "Failed to start streaming");
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

  int timeout_ms = LINKS_LOOP_INTERVAL;

  while(*running) {
    int timeout_now_ms = timeout_ms;
    timeout_ms = LINKS_LOOP_INTERVAL;

    if (links_step(all_links, timeout_now_ms, &timeout_ms) < 0) {
      links_stream(all_links, false);
      return -1;
    }
  }

  links_stream(all_links, false);
  return 0;
}

static void links_dump_buf_list(char *output, buffer_list_t *buf_list)
{
  sprintf(output + strlen(output), "%s[%dx%d/%s/%d]", \
    buf_list->name, buf_list->fmt.width, buf_list->fmt.height, \
    fourcc_to_string(buf_list->fmt.format).buf, buf_list->fmt.nbufs);
}

void links_dump(link_t *all_links)
{
  char line[4096];

  for (int n = 0; all_links[n].source; n++) {
    link_t *link = &all_links[n];

    line[0] = 0;
    links_dump_buf_list(line, link->source);
    strcat(line, " => [");
    for (int j = 0; link->sinks[j]; j++) {
      if (j > 0)
        strcat(line, ", ");
      links_dump_buf_list(line, link->sinks[j]);
    }

    for (int j = 0; j < link->n_callbacks; j++) {
      if (link->sinks[0] || j > 0)
        strcat(line, ", ");
      strcat(line, link->callbacks[j].name);
    }
    strcat(line, "]");

    LOG_INFO(NULL, "Link %d: %s", n, line);
  }
}
