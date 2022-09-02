#include "util/opts/log.h"
#include "util/opts/fourcc.h"
#include "device/device.h"
#include "device/device_list.h"

#include <signal.h>
#include <unistd.h>

log_options_t log_options = {
  .debug = true,
  .verbose = true
};

void print_formats(const char *type, device_info_formats_t *formats)
{
  if (!formats->n) {
    return;
  }

  printf("- %s: ", type);
  for (int j = 0; j < formats->n; j++) {
    printf("%s ", fourcc_to_string(formats->formats[j]).buf);
  }
  printf("\n");
}

int main(int argc, const char *argv[])
{
  device_list_t *list = device_list_v4l2();

  printf("Found %d devices\n", list->ndevices);

  for (int i = 0; i < list->ndevices; i++) {
    device_info_t * info = &list->devices[i];

    printf("Device %d: %s / %s / camera: %d, m2m: %d\n", i, info->name, info->path, info->camera, info->m2m);
    print_formats("Output / IN", &info->output_formats);
    print_formats("Capture / OUT", &info->capture_formats);
  }

  device_list_free(list);
  return 0;
}
