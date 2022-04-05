#pragma once

#include "hw/links.h"
#include "hw/device.h"

#define MAX_DEVICES 20
#define MAX_HTTP_METHODS 20

#define CAMERA_DEVICE_CAMERA 0

typedef struct camera_s {
  union {
    device_t *devices[MAX_DEVICES];
    struct {
      device_t *camera;

      struct {
        device_t *codec_jpeg;
        device_t *codec_h264;
      };

      union {
        struct {
          device_t *isp_srgb;
          device_t *isp_yuuv;
          device_t *isp_yuuv_low;
        } isp;

        struct {
          device_t *isp;
        } legacy_isp;

        struct {
          device_t *decoder;
        } decoder;
      };
    };
  };
  link_t links[MAX_DEVICES];

  unsigned width, height, format;
  unsigned nbufs, fps;
  bool allow_dma;
} camera_t;

#define CAMERA(DEVICE) camera->devices[DEVICE]

void camera_init(camera_t *camera);
void camera_close(camera_t *camera);
int camera_open(camera_t *camera, const char *path);
int camera_set_params(camera_t *camera);
int camera_run(camera_t *camera);

int camera_configure_isp(camera_t *camera, float high_div, float low_div);
int camera_configure_legacy_isp(camera_t *camera, float div);
int camera_configure_direct(camera_t *camera);
int camera_configure_decoder(camera_t *camera);
