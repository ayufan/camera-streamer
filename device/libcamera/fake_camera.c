#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

#include <linux/media.h>

#include "util/opts/log.h"

// This is special code to force `libcamera` to think that given sensor is different
// ex.: fake `arducam_64mp` to be run as `imx519`

void fake_camera_sensor(struct media_v2_topology *topology)
{
  struct media_v2_entity *ents = (struct media_v2_entity *)(intptr_t)topology->ptr_entities;
  if (!ents) {
    return;
  }

  const char *fake_camera = getenv("FAKE_CAMERA_SENSOR");
  if (!fake_camera) {
    return;
  }

  char source[256];
  sprintf(source, fake_camera);

  char *target = strstr(source, "=");
  if (!target) {
    return;
  }

  *target++ = ' ';

  for (int i = 0; i < topology->num_entities; i++) {
    if (strncmp(ents[i].name, source, target-source) != 0) {
      continue;
    }

    char name[sizeof(ents[i].name)];
    strcpy(name, target);
    strcat(name, " ");
    strcat(name, ents[i].name + (target-source));
    LOG_INFO(NULL, "Rewrote entity (%d): %s => %s", i, ents[i].name, name);
    strcpy(ents[i].name, name);
  }
}

int ioctl (int fd, unsigned long int req, ...)
{
  void *arg;
  va_list ap;
  va_start(ap, req);
  arg = va_arg(ap, void *);
  va_end(ap);
  int ret = syscall(SYS_ioctl, fd, req, arg);
  if (!ret && req == MEDIA_IOC_G_TOPOLOGY) {
    fake_camera_sensor(arg);
  }

  return ret;
}
