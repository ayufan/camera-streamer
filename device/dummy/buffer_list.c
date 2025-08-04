#include "dummy.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "util/opts/log.h"

#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>

int dummy_buffer_list_open(buffer_list_t *buf_list)
{
  int fd = -1;

  buf_list->dummy = calloc(1, sizeof(buffer_list_dummy_t));
  buf_list->dummy->fds[0] = -1;
  buf_list->dummy->fds[1] = -1;

  if (!buf_list->do_capture) {
    LOG_ERROR(buf_list, "Only capture mode supported");
  }

  if (pipe2(buf_list->dummy->fds, O_DIRECT|O_CLOEXEC) < 0) {
    LOG_INFO(buf_list, "Cannot open `pipe2`.");
    return -1;
  }

  fd = open(buf_list->dev->path, O_RDWR|O_NONBLOCK);
  if (fd < 0) {
		LOG_ERROR(buf_list, "Can't open device: %s", buf_list->dev->path);
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
		LOG_ERROR(buf_list, "Can't get fstat: %s", buf_list->dev->path);
  }

  buf_list->dummy->data = malloc(st.st_size);
  if (!buf_list->dummy->data) {
		LOG_ERROR(buf_list, "Can't allocate %" PRId64 " bytes for %s", (off64_t)st.st_size, buf_list->dev->path);
  }

  buf_list->dummy->length = read(fd, buf_list->dummy->data, st.st_size);
  if (!buf_list->dummy->data) {
		LOG_ERROR(buf_list, "Can't read %" PRId64 " bytes for %s. Only read %zu.", (off64_t)st.st_size, buf_list->dev->path, buf_list->dummy->length);
  }

  close(fd);
  return buf_list->fmt.nbufs;

error:
  close(fd);
  return -1;
}

void dummy_buffer_list_close(buffer_list_t *buf_list)
{
  if (buf_list->dummy) {
    close(buf_list->dummy->fds[0]);
    close(buf_list->dummy->fds[1]);
    free(buf_list->dummy->data);
  }

  free(buf_list->dummy);
}

int dummy_buffer_list_set_stream(buffer_list_t *buf_list, bool do_on)
{
  return 0;
}
