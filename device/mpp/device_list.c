#include "device/device_list.h"

#ifdef USE_MPP

#include "mpp.h"
#include "device/device.h"
#include "util/opts/log.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
  unsigned output_format;
  unsigned capture_format;
  const char *name;
} mpp_conversion_t;

static mpp_conversion_t mpp_decoder_conversions[] = {
  { V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_NV12, "MPP JPEG Decoder" },
  { V4L2_PIX_FMT_JPEG,  V4L2_PIX_FMT_NV12, "MPP JPEG Decoder" },
  { V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_YUV420, "MPP JPEG Decoder" },
  { V4L2_PIX_FMT_JPEG,  V4L2_PIX_FMT_YUV420, "MPP JPEG Decoder" },
  { V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_NV21, "MPP JPEG Decoder" },
  { V4L2_PIX_FMT_JPEG,  V4L2_PIX_FMT_NV21, "MPP JPEG Decoder" },
  { V4L2_PIX_FMT_H264,  V4L2_PIX_FMT_NV12, "MPP H264 Decoder" },
  { V4L2_PIX_FMT_H264,  V4L2_PIX_FMT_YUV420, "MPP H264 Decoder" },
  { V4L2_PIX_FMT_H264,  V4L2_PIX_FMT_NV21, "MPP H264 Decoder" },
  { V4L2_PIX_FMT_HEVC,  V4L2_PIX_FMT_NV12, "MPP HEVC Decoder" },
  { V4L2_PIX_FMT_HEVC,  V4L2_PIX_FMT_YUV420, "MPP HEVC Decoder" },
  { V4L2_PIX_FMT_VP8,   V4L2_PIX_FMT_NV12, "MPP VP8 Decoder" },
  { V4L2_PIX_FMT_VP9,   V4L2_PIX_FMT_NV12, "MPP VP9 Decoder" },
  { 0, 0, NULL }
};

static mpp_conversion_t mpp_encoder_conversions[] = {
  { V4L2_PIX_FMT_NV12,   V4L2_PIX_FMT_JPEG,  "MPP JPEG Encoder" },
  { V4L2_PIX_FMT_NV12,   V4L2_PIX_FMT_MJPEG, "MPP JPEG Encoder" },
  { V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_JPEG,  "MPP JPEG Encoder" },
  { V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_MJPEG, "MPP JPEG Encoder" },
  { V4L2_PIX_FMT_YUYV,   V4L2_PIX_FMT_JPEG,  "MPP JPEG Encoder" },
  { V4L2_PIX_FMT_YUYV,   V4L2_PIX_FMT_MJPEG, "MPP JPEG Encoder" },
  { V4L2_PIX_FMT_NV12,   V4L2_PIX_FMT_H264,  "MPP H264 Encoder" },
  { V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_H264,  "MPP H264 Encoder" },
  { V4L2_PIX_FMT_YUYV,   V4L2_PIX_FMT_H264,  "MPP H264 Encoder" },
  { V4L2_PIX_FMT_NV12,   V4L2_PIX_FMT_HEVC,  "MPP HEVC Encoder" },
  { V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_HEVC,  "MPP HEVC Encoder" },
  { 0, 0, NULL }
};

static bool mpp_is_available(void)
{
  MppCtx ctx = NULL;
  MppApi *mpi = NULL;

  MPP_RET ret = mpp_create(&ctx, &mpi);
  if (ret != MPP_OK) {
    return false;
  }

  mpp_destroy(ctx);
  return true;
}

device_info_t *mpp_add_conversion(device_list_t *list, int mode, mpp_conversion_t *conv)
{
  device_info_t info = {NULL};

  info.name = strdup(conv->name);
  asprintf(&info.path, "%d:%d:%d", mode, conv->output_format, conv->capture_format);
  info.m2m = true;
  info.output_formats.formats = calloc(1, sizeof(unsigned));
  info.output_formats.formats[0] = conv->output_format;
  info.output_formats.n = 1;
  info.capture_formats.formats = calloc(1, sizeof(unsigned));
  info.capture_formats.formats[0] = conv->capture_format;
  info.capture_formats.n = 1;
  info.open = device_mpp_open;

  list->ndevices++;
  list->devices = realloc(list->devices, sizeof(info) * list->ndevices);
  list->devices[list->ndevices-1] = info;

  return &list->devices[list->ndevices-1];
}
#endif

void device_list_mpp(device_list_t *list)
{
#ifdef USE_MPP
  if (!list) {
    return;
  }

  if (!mpp_is_available()) {
    LOG_INFO(NULL, "Rockchip MPP not available");
    return;
  }

  for (int i = 0; mpp_decoder_conversions[i].name; i++) {
    mpp_conversion_t *conv = &mpp_decoder_conversions[i];
    mpp_add_conversion(list, 0, conv);
  }

  for (int i = 0; mpp_encoder_conversions[i].name; i++) {
    mpp_conversion_t *conv = &mpp_encoder_conversions[i];
    mpp_add_conversion(list, 1, conv);
  }

  LOG_INFO(NULL, "Rockchip MPP devices added to device list");
#endif
}

