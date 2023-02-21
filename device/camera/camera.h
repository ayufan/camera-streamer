#pragma once

#include "device/links.h"
#include "device/device.h"

#define MAX_DEVICES 20
#define MAX_RESCALLERS 4
#define MAX_HTTP_METHODS 20

#define CAMERA_DEVICE_CAMERA 0
#define CAMERA_OPTIONS_LENGTH 4096

typedef enum {
  CAMERA_V4L2 = 0,
  CAMERA_LIBCAMERA,
  CAMERA_DUMMY
} camera_type_t;

typedef struct camera_output_options_s {
  bool disabled;
  unsigned height;
  char options[CAMERA_OPTIONS_LENGTH];
} camera_output_options_t;

typedef struct camera_options_s {
  char path[256];
  unsigned width, height, format;
  unsigned nbufs, fps;
  camera_type_t type;
  bool allow_dma;
  float high_res_factor;
  float low_res_factor;
  bool auto_focus;
  unsigned auto_reconnect;
  union {
    bool vflip;
    unsigned vflip_align;
  };
  union {
    bool hflip;
    unsigned hflip_align;
  };

  char options[CAMERA_OPTIONS_LENGTH];
  bool list_options;

  struct {
    char options[CAMERA_OPTIONS_LENGTH];
  } isp;

  camera_output_options_t snapshot;
  camera_output_options_t stream;
  camera_output_options_t video;
} camera_options_t;

typedef struct camera_s {
  const char *name;

  camera_options_t options;

  union {
    device_t *devices[MAX_DEVICES];
    struct {
      device_t *camera;
      device_t *decoder; // decode JPEG/H264 into YUVU
      device_t *isp;
      device_t *rescallers[3];
      device_t *codec_snapshot;
      device_t *codec_stream;
      device_t *codec_video;
    };
  };

  struct device_list_s *device_list;

  link_t links[MAX_DEVICES];
  int nlinks;
} camera_t;

#define CAMERA(DEVICE) camera->devices[DEVICE]

camera_t *camera_open(camera_options_t *camera);
int camera_set_params(camera_t *camera);
void camera_close(camera_t **camera);
int camera_run(camera_t *camera);

link_t *camera_ensure_capture(camera_t *camera, buffer_list_t *capture);
void camera_capture_add_output(camera_t *camera, buffer_list_t *capture, buffer_list_t *output);
void camera_capture_add_callbacks(camera_t *camera, buffer_list_t *capture, link_callbacks_t callbacks);

int camera_configure_input(camera_t *camera);
int camera_configure_pipeline(camera_t *camera, buffer_list_t *camera_capture);

buffer_list_t *camera_configure_isp(camera_t *camera, buffer_list_t *src_capture);
buffer_list_t *camera_configure_decoder(camera_t *camera, buffer_list_t *src_capture);
unsigned camera_rescaller_align_size(unsigned target_height);
buffer_list_t *camera_configure_rescaller(camera_t *camera, buffer_list_t *src_capture, const char *name, unsigned target_height, unsigned formats[]);
int camera_configure_output(camera_t *camera, const char *name, unsigned target_height, unsigned formats[], link_callbacks_t callbacks, device_t **device);
