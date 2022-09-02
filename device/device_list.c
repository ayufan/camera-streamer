#include "device/device_list.h"

#include <stddef.h>
#include <stdlib.h>

bool device_info_has_format(device_info_t *info, bool capture, unsigned format)
{
  if (!info) {
    return false;
  }

  device_info_formats_t *formats = capture ? &info->capture_formats : &info->output_formats;

  for (int i = 0; i < formats->n; i++) {
    if (formats->formats[i] == format) {
      return true;
    }
  }

  return false;
}

device_info_t *device_list_find_m2m_format(device_list_t *list, unsigned output, unsigned capture)
{
  if (!list)
    return NULL;

  for (int i = 0; i < list->ndevices; i++) {
    device_info_t *info = &list->devices[i];

    if (info->m2m && device_info_has_format(info, false, output) && device_info_has_format(info, true, capture)) {
      return info;
    }
  }

  return NULL;
}

device_info_t *device_list_find_m2m_formats(device_list_t *list, unsigned output, unsigned capture_formats[], unsigned *found_format)
{
  for (int i = 0; capture_formats[i]; i++) {
    device_info_t *info = device_list_find_m2m_format(list, output, capture_formats[i]);
    if (info) {
      if (found_format) {
        *found_format = capture_formats[i];
      }
      return info;
    }
  }

  return NULL;
}

void device_list_free(device_list_t *list)
{
  if (!list)
    return;

  for (int i = 0; i < list->ndevices; i++) {
    device_info_t *info = &list->devices[i];
    free(info->name);
    free(info->path);
    free(info->output_formats.formats);
    free(info->capture_formats.formats);
  }

  free(list);
}
