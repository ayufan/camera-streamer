#include "camera.h"

#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/links.h"
#include "opts/log.h"
#include "opts/fourcc.h"
#include "device/buffer_list.h"
#include "http/http.h"

link_t *camera_ensure_capture(camera_t *camera, buffer_list_t *capture)
{
  for (int i = 0; i < camera->nlinks; i++) {
    if (camera->links[i].source == capture) {
      return &camera->links[i];
    }
  }

  link_t *link = &camera->links[camera->nlinks++];
  link->source = capture;
  return link;
}

void camera_capture_add_output(camera_t *camera, buffer_list_t *capture, buffer_list_t *output)
{
  link_t *link = camera_ensure_capture(camera, capture);

  int nsinks;
  for (nsinks = 0; link->sinks[nsinks]; nsinks++);
  link->sinks[nsinks] = output;
}

void camera_capture_set_callbacks(camera_t *camera, buffer_list_t *capture, link_callbacks_t callbacks)
{
  link_t *link = camera_ensure_capture(camera, capture);
  link->callbacks = callbacks;
}
