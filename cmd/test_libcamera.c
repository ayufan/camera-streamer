#include "opts/opts.h"
#include "opts/log.h"
#include "opts/fourcc.h"
#include "device/camera/camera.h"

log_options_t log_options = {
  .debug = false,
  .verbose = false
};

int main(int argc, char *argv[])
{
  device_t *dev = NULL;

  dev = device_libcamera_open("CAMERA", "/dev/video0");
  if (!dev) {
    printf("Failed to open libcamera\n");
    return -1;
  }

  printf("Opened libcamera\n");

  device_close(dev);
  return 0;
}
