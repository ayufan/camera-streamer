#include "camera.h"

#include "device/buffer_list.h"
#include "device/device.h"
#include "util/opts/log.h"

bool camera_uses_crop(camera_crop_t *crop)
{
  if (!crop) {
    return false;
  }

  if (crop->left <= 0 && crop->right <= 0 &&
    crop->top <= 0 && crop->bottom <= 0) {
    return false;
  }

  return true;
}

int camera_configure_crop(device_t *dev, buffer_format_t fmt, camera_crop_t *crop)
{
  if (!camera_uses_crop(crop))
    return 0;

  int x = fmt.width * crop->left;
  int y = fmt.height * crop->top;
  int width = fmt.width * (1.0 - crop->left - crop->right);
  int height = fmt.height * (1.0 - crop->top - crop->bottom);

  if (x < 0 || y < 0 || width <= 1 || height <= 1) {
    LOG_INFO(dev, "CROP settings do not make sense: %dx%d/(%dx%d)",
      x, y, width, height);
    return -1;
  }

  LOG_INFO(dev, "CROP: %dx%d/(%dx%d)",
    x, y, width, height);

  buffer_rect_t rect = {
    .x = x,
    .y = y,
    .width = width,
    .height = height,
  };

  return device_set_target_crop(dev, &rect);
}
