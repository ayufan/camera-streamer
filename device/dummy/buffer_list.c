#include "dummy.h"
#include "device/buffer_list.h"

#include <stdlib.h>

int dummy_buffer_list_open(buffer_list_t *buf_list, const char *path, unsigned width, unsigned height, unsigned format, unsigned bytesperline, int nbufs)
{
  buf_list->dummy = calloc(1, sizeof(buffer_list_dummy_t));
  return 0;
}

void dummy_buffer_list_close(buffer_list_t *buf_list)
{
  free(buf_list->dummy);
}

int dummy_buffer_list_set_buffers(buffer_list_t *buf_list, int nbufs)
{
  return -1;
}

int dummy_buffer_list_set_stream(buffer_list_t *buf_list, bool do_on)
{
  return -1;
}
