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

typedef struct link_pool_s
{
  struct pollfd fds[N_FDS];
  link_t *links[N_FDS];
  buffer_list_t *buf_lists[N_FDS];
  int max_timeout_ms;
} link_pool_t;

static bool link_needs_buffer_by_callbacks(link_t *link)
{
  bool needs = false;

  for (int j = 0; j < link->n_callbacks; j++) {
    if (link->callbacks[j].check_streaming && link->callbacks[j].check_streaming()) {
      needs = true;
    }

    if (link->callbacks[j].buf_lock && buffer_lock_needs_buffer(link->callbacks[j].buf_lock)) {
      needs = true;
    }
  }

  return needs;
}

static int links_build_fds(link_t *all_links, link_pool_t *link_pool)
{
  int n = 0, nlinks = 0;

  uint64_t now_us __attribute__((unused)) = get_monotonic_time_us(NULL, NULL);

  for (nlinks = 0; all_links[nlinks].capture_list; nlinks++);

  // This traverses in reverse order as it requires to first fix outputs
  // and go back into captures

  for (int i = nlinks; i-- > 0; ) {
    link_t *link = &all_links[i];
    buffer_list_t *capture_list = link->capture_list;

    if (n >= N_FDS) {
      return -EINVAL;
    }
    if (!capture_list->streaming) {
      continue;
    }

    bool paused = true;

    if (link_needs_buffer_by_callbacks(link)) {
      paused = false;
    }

    for (int j = 0; link->output_lists[j]; j++) {
      buffer_list_t *sink = link->output_lists[j];

      if (n >= N_FDS) {
        return -EINVAL;
      }
      if (!sink->streaming) {
        continue;
      }

      // Can something be dequeued?
      if (buffer_list_pollfd(sink, &link_pool->fds[n], true) == 0) {
        link_pool->buf_lists[n] = sink;
        link_pool->links[n] = NULL;
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

    capture_list->dev->paused = paused;

    int count_enqueued = buffer_list_count_enqueued(capture_list);
    bool can_dequeue = count_enqueued > 0;

#ifndef QUEUE_ON_CAPTURE
    if (now_us - capture_list->last_dequeued_us < capture_list->fmt.interval_us) {
      can_dequeue = false;
      link_pool->max_timeout_ms = MIN(link_pool->max_timeout_ms, (capture_list->last_dequeued_us + capture_list->fmt.interval_us - now_us) / 1000);
    }
#endif

    if (buffer_list_pollfd(capture_list, &link_pool->fds[n], can_dequeue) == 0) {
      link_pool->buf_lists[n] = capture_list;
      link_pool->links[n] = link;
      n++;
    }
  }

  return n;
}

static bool links_output_list_can_enqueue(buffer_list_t *output_list)
{
  int current = buffer_list_count_enqueued(output_list);

  if (output_list->do_capture) {
    perror("should not happen");
  }

  int capture_max = 0;

  for (int i = 0; i < output_list->dev->n_capture_list; i++) {
    int capture_count = buffer_list_count_enqueued(output_list->dev->capture_lists[i]);
    if (capture_max < capture_count)
      capture_max = capture_count;
  }

  // only enqueue on output, if there are already captures (and there's more of them)
  if (capture_max <= current) {
    LOG_DEBUG(output_list, "Skipping enqueue of output (output_enqueued=%d, capture_enqueued=%d)",
      current, capture_max);
    return false;
  }

  return true;
}

static int links_enqueue_from_capture_list(buffer_list_t *capture_list, link_t *link)
{
  if (!link) {
    LOG_ERROR(capture_list, "Missing link for capture_list");
  }

  buffer_t *buf = buffer_list_dequeue(capture_list);
  if (!buf) {
    LOG_ERROR(capture_list, "No buffer dequeued from capture_list?");
  }

  for (int j = 0; j < link->n_callbacks; j++) {
    if (link->callbacks[j].validate_buffer && !link->callbacks[j].validate_buffer(link, buf)) {
      LOG_DEBUG(capture_list, "Buffer rejected by validation");
      return 0;
    }
  }

  bool dropped = false;

  for (int j = 0; link->output_lists[j]; j++) {
    if (link->output_lists[j]->dev->paused) {
      continue;
    }
    if (links_output_list_can_enqueue(link->output_lists[j])) {
      buffer_list_enqueue(link->output_lists[j], buf);
    } else {
      dropped = true;
    }
  }

  if (dropped) {
    capture_list->stats.dropped++;
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

static int links_dequeue_from_output_list(buffer_list_t *output_list)
{
  buffer_t *buf = buffer_list_dequeue(output_list);
  if (!buf) {
    LOG_ERROR(buf, "No buffer dequeued from sink?");
  }

  return 0;

error:
  return -1;
}

static void print_pollfds(struct pollfd *fds, int n)
{
  if (!getenv("DEBUG_FDS")) {
    return;
  }

  for (int i = 0; i < n; i++) {
    printf("poll(i=%i, fd=%d, events=%08x, revents=%08x)\n", i, fds[i].fd, fds[i].events, fds[i].revents);
  }
  printf("pollfds = %d\n", n);
}

static int links_step(link_t *all_links, int timeout_now_ms, int *timeout_next_ms)
{
  link_pool_t pool = {
    .fds = {{0}},
    .max_timeout_ms = timeout_now_ms
  };

  int n = links_build_fds(all_links, &pool);
  print_pollfds(pool.fds, n);
  int ret = poll(pool.fds, n, timeout_now_ms);
  print_pollfds(pool.fds, n);

  uint64_t now_us __attribute__((unused)) = get_monotonic_time_us(NULL, NULL);

  if (ret < 0 && errno != EINTR) {
    return errno;
  }

  for (int i = 0; i < n; i++) {
    buffer_list_t *buf_list = pool.buf_lists[i];
    link_t *link = pool.links[i];

    LOG_DEBUG(buf_list, "pool event=%08x revent=%s%s%s%s%s%08x streaming=%d enqueued=%d/%d paused=%d",
      pool.fds[i].events,
      !pool.fds[i].revents ? "NONE/" : "",
      pool.fds[i].revents & POLLIN ? "IN/" : "",
      pool.fds[i].revents & POLLOUT ? "OUT/" : "",
      pool.fds[i].revents & POLLHUP ? "HUP/" : "",
      pool.fds[i].revents & POLLERR ? "ERR/" : "",
      pool.fds[i].revents,
      buf_list->streaming,
      buffer_list_count_enqueued(buf_list),
      buf_list->nbufs,
      buf_list->dev->paused);

    if (pool.fds[i].revents & POLLIN) {
      if (links_enqueue_from_capture_list(buf_list, link) < 0) {
        return -1;
      }
    }

    // Dequeue buffers that were processed
    if (pool.fds[i].revents & POLLOUT) {
      if (links_dequeue_from_output_list(buf_list) < 0) {
        return -1;
      }
    }

    if (pool.fds[i].revents & POLLHUP) {
      LOG_INFO(buf_list, "Device disconnected.");
      return -1;
    }

    if (pool.fds[i].revents & POLLERR) {
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

static int links_stream(link_t *all_links, bool do_stream)
{
  for (int i = 0; all_links[i].capture_list; i++) {
    bool streaming = true;
    link_t *link = &all_links[i];

    if (buffer_list_set_stream(link->capture_list, streaming) < 0) {
      LOG_ERROR(link->capture_list, "Failed to start streaming");
    }

    for (int j = 0; link->output_lists[j]; j++) {
      if (buffer_list_set_stream(link->output_lists[j], streaming) < 0) {
        LOG_ERROR(link->output_lists[j], "Failed to start streaming");
      }
    }
  }

  return 0;

error:
  return -1;
}

static void links_refresh_stats(link_t *all_links, uint64_t *last_refresh_us)
{
  uint64_t now_us = get_monotonic_time_us(NULL, NULL);

  if (now_us - *last_refresh_us < 1000*1000)
    return;

  *last_refresh_us = now_us;

  if (log_options.stats) {
    printf("Statistics:");

    for (int i = 0; all_links[i].capture_list; i++) {
      buffer_list_t *capture_list = all_links[i].capture_list;
      buffer_stats_t *now = &capture_list->stats;
      buffer_stats_t *prev = &capture_list->stats_last;

      printf(" [%8s %2d FPS/%2d D/%3dms/%3dms/%c/O%d:C%d]",
        capture_list->dev->name,
        (now->frames - prev->frames) / log_options.stats,
        (now->dropped - prev->dropped) / log_options.stats,
        capture_list->last_capture_time_us > 0 ? capture_list->last_capture_time_us / 1000 : -1,
        capture_list->last_in_queue_time_us > 0 ? capture_list->last_in_queue_time_us / 1000 : -1,
        capture_list->streaming ? (capture_list->dev->paused ? 'P' : 'S') : 'X',
        capture_list->dev->output_list ? buffer_list_count_enqueued(capture_list->dev->output_list) : 0,
        buffer_list_count_enqueued(capture_list)
      );
    }

    printf("\n");
    fflush(stdout);
  }

  for (int i = 0; all_links[i].capture_list; i++) {
    buffer_list_t *capture_list = all_links[i].capture_list;
    capture_list->stats_last = capture_list->stats;

    if (now_us - capture_list->last_dequeued_us > 1000) {
      capture_list->last_capture_time_us = 0;
      capture_list->last_in_queue_time_us = 0;
    }
  }
}

int links_loop(link_t *all_links, bool *running)
{
  *running = true;

  if (links_stream(all_links, true) < 0) {
    return -1;
  }

  int timeout_ms = LINKS_LOOP_INTERVAL;
  uint64_t last_refresh_us = get_monotonic_time_us(NULL, NULL);
  int ret = 0;

  while(*running && ret == 0) {
    int timeout_now_ms = timeout_ms;
    timeout_ms = LINKS_LOOP_INTERVAL;

    ret = links_step(all_links, timeout_now_ms, &timeout_ms);
    links_refresh_stats(all_links, &last_refresh_us);
  }

  links_stream(all_links, false);
  return ret;
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

  for (int n = 0; all_links[n].capture_list; n++) {
    link_t *link = &all_links[n];

    line[0] = 0;
    links_dump_buf_list(line, link->capture_list);
    strcat(line, " => [");
    for (int j = 0; link->output_lists[j]; j++) {
      if (j > 0)
        strcat(line, ", ");
      links_dump_buf_list(line, link->output_lists[j]);
    }

    for (int j = 0; j < link->n_callbacks; j++) {
      if (link->output_lists[0] || j > 0)
        strcat(line, ", ");
      strcat(line, link->callbacks[j].name);
    }
    strcat(line, "]");

    LOG_INFO(NULL, "Link %d: %s", n, line);
  }
}
