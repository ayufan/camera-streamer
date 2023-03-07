#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/links.h"
#include "device/camera/camera.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"
#include "output/output.h"

#include <sys/stat.h>

static void debug_capture_on_buffer(buffer_t *buf)
{
  if (!buf) {
    return;
  }

  char path[256];
  sprintf(path, "%s/decoder_capture.%d.%s", getenv("CAMERA_DEBUG_CAPTURE"), buf->index, fourcc_to_string(buf->buf_list->fmt.format).buf);

  FILE *fp = fopen(path, "wb");
  if (!fp) {
    return;
  }

  fwrite(buf->start, 1, buf->used, fp);
  fclose(fp);
}

static link_callbacks_t debug_capture_callbacks = {
  .name = "DEBUG-CAPTURE",
  .on_buffer = debug_capture_on_buffer
};

void camera_debug_capture(camera_t *camera, buffer_list_t *capture)
{
  if (getenv("CAMERA_DEBUG_CAPTURE")) {
    mkdir(getenv("CAMERA_DEBUG_CAPTURE"), 0755);
    camera_capture_add_callbacks(camera, capture, debug_capture_callbacks);
  }
}
