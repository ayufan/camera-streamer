#pragma once

#include <stdbool.h>

typedef struct device_info_formats_s {
  unsigned *formats;
  unsigned n;
} device_info_formats_t;

typedef struct device_info_s {
  char *name;
  char *path;

  bool camera;
  bool m2m;

  device_info_formats_t output_formats;
  device_info_formats_t capture_formats;
} device_info_t;

typedef struct device_list_s {
  device_info_t *devices;
  int ndevices;
} device_list_t;

device_list_t *device_list_v4l2();
bool device_info_has_format(device_info_t *info, bool capture, unsigned format);
device_info_t *device_list_find_m2m_format(device_list_t *list, unsigned output, unsigned capture);
device_info_t *device_list_find_m2m_formats(device_list_t *list, unsigned output, unsigned capture_formats[], unsigned *found_format);
void device_list_free(device_list_t *list);
