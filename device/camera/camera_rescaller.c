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

static unsigned camera_rescaller_align_size(unsigned size, int align_size)
{
  if (align_size > 0)
    return (size + align_size - 1) / align_size * align_size;
  else if (align_size < 0)
    return size / -align_size * -align_size;
  else
    return size;
}

void camera_get_scaled_resolution2(unsigned in_width, unsigned in_height, unsigned proposed_height, unsigned *target_width, unsigned *target_height, int align_size)
{
  proposed_height = MIN(proposed_height, in_height);

  *target_height = camera_rescaller_align_size(proposed_height, align_size);
  *target_height = MIN(*target_height, MAX_RESCALLER_SIZE);

  // maintain aspect ratio on target width
  *target_width = camera_rescaller_align_size(*target_height * in_width / in_height, align_size);

  // if width is larger then rescaller, try to maintain scale down height
  if (*target_width > MAX_RESCALLER_SIZE) {
    *target_width = MAX_RESCALLER_SIZE;
    *target_height = camera_rescaller_align_size(*target_width * in_height / in_width, align_size);
  }
}

bool camera_get_scaled_resolution(buffer_format_t capture_format, camera_output_options_t *options, buffer_format_t *format, int align_size)
{
  if (options->disabled)
    return false;

  camera_get_scaled_resolution2(
    capture_format.width,
    capture_format.height,
    options->height,
    &format->width,
    &format->height,
    align_size
  );
  return format->height > 0;
}

buffer_list_t *camera_try_rescaller(camera_t *camera, buffer_list_t *src_capture, const char *name, unsigned target_height, unsigned target_format)
{
  device_info_t *device_info = device_list_find_m2m_format(camera->device_list, src_capture->fmt.format, target_format);

  if (!device_info) {
    return NULL;
  }

  if (target_height > src_capture->fmt.height) {
    LOG_INFO(src_capture, "Upscaling from %dp to %dp does not make sense. Lowering to %dp.",
      src_capture->fmt.height, target_height, src_capture->fmt.height);
  }

  char name2[256];
  sprintf(name2, "RESCALLER:%s", name);

  device_t *device = device_v4l2_open(name2, device_info->path);

  buffer_list_t *rescaller_output = device_open_buffer_list_output(
    device, src_capture);

  buffer_format_t target_fmt = {
    .format = target_format
  };

  camera_get_scaled_resolution2(
    src_capture->fmt.width, src_capture->fmt.height,
    target_height,
    &target_fmt.width, &target_fmt.height,
    RESCALLER_BLOCK_SIZE
  );

  buffer_list_t *rescaller_capture = device_open_buffer_list_capture(
    device, NULL, rescaller_output, target_fmt, true);

  if (!rescaller_capture) {
    device_close(device);
    return NULL;
  }

  camera_capture_add_output(camera, src_capture, rescaller_output);
  return rescaller_capture;
}

buffer_list_t *camera_configure_rescaller(camera_t *camera, buffer_list_t *src_capture, const char *name, unsigned target_height, unsigned formats[])
{
  int rescallers = 0;
  for ( ; rescallers < MAX_RESCALLERS && camera->rescallers[rescallers]; rescallers++);
  if (rescallers == MAX_RESCALLERS) {
    return NULL;
  }

  buffer_list_t *rescaller_capture = camera_try_rescaller(camera, src_capture, name, target_height, src_capture->fmt.format);

  for (int i = 0; !rescaller_capture && formats[i]; i++) {
    rescaller_capture = camera_try_rescaller(camera, src_capture, name, target_height, formats[i]);
  }

  if (!rescaller_capture) {
    LOG_INFO(src_capture, "Cannot find rescaller to scale from '%s' to 'YUYV'", fourcc_to_string(src_capture->fmt.format).buf);
    return NULL;
  }

  camera->rescallers[rescallers] = rescaller_capture->dev;
  return rescaller_capture;
}
