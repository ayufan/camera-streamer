#include "opts/opts.h"
#include "opts/log.h"
#include "opts/fourcc.h"
#include "device/camera/camera.h"

log_options_t log_options = {
  .debug = true,
  .verbose = false
};

int main(int argc, char *argv[])
{
  device_t *dev = NULL;

  dev = device_libcamera_open("CAMERA", "/base/soc/i2c0mux/i2c@1/imx519@1a");
  if (!dev) {
    printf("Failed to open libcamera\n");
    return -1;
  }

  if (device_open_buffer_list(dev, true, 2328, 1748, V4L2_PIX_FMT_VYUY, 0, 4, true) < 0) {
    printf("Failed to open buffer list\n");
    return -1;
  }

  printf("Opened libcamera\n");

  device_close(dev);
  return 0;
}
