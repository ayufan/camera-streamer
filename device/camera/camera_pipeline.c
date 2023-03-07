#include "camera.h"

#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/device_list.h"
#include "device/links.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"
#include "device/buffer_list.h"
#include "util/http/http.h"
#include "output/output.h"

static unsigned snapshot_formats[] =
{
  V4L2_PIX_FMT_JPEG,
  V4L2_PIX_FMT_MJPEG,
  0
};

static link_callbacks_t snapshot_callbacks =
{
  .name = "SNAPSHOT-CAPTURE",
  .buf_lock = &snapshot_lock
};

static link_callbacks_t stream_callbacks =
{
  .name = "STREAM-CAPTURE",
  .buf_lock = &stream_lock
};

static unsigned video_formats[] =
{
  V4L2_PIX_FMT_H264,
  0
};

static link_callbacks_t video_callbacks =
{
  .name = "VIDEO-CAPTURE",
  .buf_lock = &video_lock
};

int camera_configure_pipeline(camera_t *camera, buffer_list_t *camera_capture)
{
  camera_capture->do_timestamps = true;

  camera_debug_capture(camera, camera_capture);

  if (camera_configure_output(camera, camera_capture, "SNAPSHOT", &camera->options.snapshot,
    snapshot_formats, snapshot_callbacks, &camera->codec_snapshot) < 0) {
    return -1;
  }

  if (camera_configure_output(camera, camera_capture, "STREAM", &camera->options.stream,
    snapshot_formats, stream_callbacks, &camera->codec_stream) < 0) {
    return -1;
  }

  if (camera_configure_output(camera, camera_capture, "VIDEO", &camera->options.video,
    video_formats, video_callbacks, &camera->codec_video) < 0) {
    return -1;
  }

  return 0;
}
