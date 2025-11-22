#include "device/device_list.h"

#ifdef USE_MPP

#include "mpp.h"
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

static void add_format_if_missing(device_info_formats_t *formats, unsigned format)
{
  for (int i = 0; i < formats->n; i++) {
    if (formats->formats[i] == format) {
      return;
    }
  }

  formats->n++;
  formats->formats = realloc(formats->formats, sizeof(formats->formats[0]) * formats->n);
  formats->formats[formats->n - 1] = format;
}

static device_info_t *find_or_create_mpp_device(device_list_t *list, const char *name)
{
  for (int i = 0; i < list->ndevices; i++) {
    if (list->devices[i].path && strncmp(list->devices[i].path, "mpp://", 6) == 0) {
      if (strcmp(list->devices[i].name, name) == 0) {
        return &list->devices[i];
      }
    }
  }

  list->ndevices++;
  list->devices = realloc(list->devices, sizeof(device_info_t) * list->ndevices);
  device_info_t *info = &list->devices[list->ndevices - 1];
  memset(info, 0, sizeof(*info));
  info->name = strdup(name);
  info->m2m = true;

  return info;
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
    device_info_t *info = find_or_create_mpp_device(list, conv->name);

    if (!info->path) {
      info->path = strdup("mpp://decoder");
    }

    add_format_if_missing(&info->output_formats, conv->output_format);
    add_format_if_missing(&info->capture_formats, conv->capture_format);
  }

  for (int i = 0; mpp_encoder_conversions[i].name; i++) {
    mpp_conversion_t *conv = &mpp_encoder_conversions[i];
    device_info_t *info = find_or_create_mpp_device(list, conv->name);

    if (!info->path) {
      info->path = strdup("mpp://encoder");
    }

    add_format_if_missing(&info->output_formats, conv->output_format);
    add_format_if_missing(&info->capture_formats, conv->capture_format);
  }

  LOG_INFO(NULL, "Rockchip MPP devices added to device list");
#endif
}

