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
  V4L2_PIX_FMT_MJPEG,
  V4L2_PIX_FMT_JPEG,
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

int camera_configure_pipeline(camera_t *camera, buffer_list_t *capture)
{
  if (capture) {
    capture->do_timestamps = true;

    if (camera->options.fps > 0) {
      capture->fmt.interval_us = 1000 * 1000 / camera->options.fps;
    }

    switch (capture->fmt.format) {
    case V4L2_PIX_FMT_SRGGB10P:
    case V4L2_PIX_FMT_SGRBG10P:
    case V4L2_PIX_FMT_SRGGB10:
    case V4L2_PIX_FMT_SGRBG10:
      camera_configure_isp(camera, capture);
      break;

    case V4L2_PIX_FMT_MJPEG:
    case V4L2_PIX_FMT_H264:
      camera_configure_decoder(camera, capture);
      break;
    }
  }

  if (!camera->options.snapshot.disabled && camera_configure_output(
    camera, "SNAPSHOT", camera->options.snapshot.height, snapshot_formats, snapshot_callbacks, &camera->codec_snapshot) < 0) {
    return -1;
  }

  if (!camera->options.stream.disabled && camera_configure_output(
    camera, "STREAM", camera->options.stream.height, snapshot_formats, stream_callbacks, &camera->codec_stream) < 0) {
    return -1;
  }

  if (!camera->options.video.disabled && camera_configure_output(
    camera, "VIDEO", camera->options.video.height, video_formats, video_callbacks, &camera->codec_video) < 0) {
    return -1;
  }

  return 0;
}
