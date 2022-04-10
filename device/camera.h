#pragma once

#include "device/links.h"
#include "device/hw/device.h"

#define MAX_DEVICES 20
#define MAX_HTTP_METHODS 20

#define CAMERA_DEVICE_CAMERA 0
#define CAMERA_OPTIONS_LENGTH 4096

typedef struct camera_options_s {
  char path[256];
  unsigned width, height, format;
  unsigned nbufs, fps;
  bool allow_dma;
  float high_res_factor;
  float low_res_factor;

  char options[CAMERA_OPTIONS_LENGTH];

  struct {
    char options[CAMERA_OPTIONS_LENGTH];
  } isp;
} camera_options_t;

typedef struct camera_s {
  const char *name;

  camera_options_t options;

  union {
    device_t *devices[MAX_DEVICES];
    struct {
      device_t *camera;
      device_t *decoder; // decode JPEG/H264 into YUVU
      device_t *legacy_isp; // convert pRAA/YUVU into YUVU
      device_t *isp_srgb;
      device_t *isp_yuuv;
      device_t *isp_yuuv_lowres;
      device_t *codec_jpeg; // encode YUVU into JPEG
      device_t *codec_h264; // encode YUVU into H264
      device_t *codec_jpeg_lowres; // encode YUVU into JPEG
      device_t *codec_h264_lowres; // encode YUVU into H264
    };
  };

  link_t links[MAX_DEVICES];
  int nlinks;
} camera_t;

#define CAMERA(DEVICE) camera->devices[DEVICE]

camera_t *camera_open(camera_options_t *camera);
int camera_set_params(camera_t *camera);
void camera_close(camera_t *camera);
int camera_run(camera_t *camera);

int camera_configure_isp(camera_t *camera, float high_div, float low_div);
int camera_configure_legacy_isp(camera_t *camera, float div);
int camera_configure_direct(camera_t *camera);
int camera_configure_decoder(camera_t *camera);
