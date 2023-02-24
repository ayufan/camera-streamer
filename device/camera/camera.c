#include "camera.h"

#include "device/device.h"
#include "device/device_list.h"
#include "device/buffer_list.h"
#include "device/buffer_lock.h"
#include "device/links.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"

camera_t *camera_open(camera_options_t *options)
{
  camera_t *camera = calloc(1, sizeof(camera_t));
  camera->name = "CAMERA";
  camera->options = *options;
  camera->device_list = device_list_v4l2();

  if (camera_configure_input(camera) < 0) {
    goto error;
  }

  if (camera_set_params(camera) < 0) {
    goto error;
  }

  links_dump(camera->links);

  return camera;

error:
  camera_close(&camera);
  return NULL;
}

void camera_close(camera_t **camerap)
{
  if (!camerap || !*camerap)
    return;

  camera_t *camera = *camerap;
  *camerap = NULL;

  for (int i = MAX_DEVICES; i-- > 0; ) {
    link_t *link = &camera->links[i];

    for (int j = 0; j < link->n_callbacks; j++) {
      if (link->callbacks[j].on_buffer) {
        link->callbacks[j].on_buffer(NULL);
        link->callbacks[j].on_buffer = NULL;
      }
      if (link->callbacks[j].buf_lock) {
        buffer_lock_capture(link->callbacks[j].buf_lock, NULL);
        link->callbacks[j].buf_lock = NULL;
      }
    }
  }

  for (int i = MAX_DEVICES; i-- > 0; ) {
    if (camera->devices[i]) {
      device_close(camera->devices[i]);
      camera->devices[i] = NULL;
    }
  }

  device_list_free(camera->device_list);
  free(camera);
}

link_t *camera_ensure_capture(camera_t *camera, buffer_list_t *capture)
{
  for (int i = 0; i < camera->nlinks; i++) {
    if (camera->links[i].capture_list == capture) {
      return &camera->links[i];
    }
  }

  link_t *link = &camera->links[camera->nlinks++];
  link->capture_list = capture;
  return link;
}

void camera_capture_add_output(camera_t *camera, buffer_list_t *capture, buffer_list_t *output)
{
  link_t *link = camera_ensure_capture(camera, capture);
  ARRAY_APPEND(link->output_lists, link->n_output_lists, output);
}

void camera_capture_add_callbacks(camera_t *camera, buffer_list_t *capture, link_callbacks_t callbacks)
{
  link_t *link = camera_ensure_capture(camera, capture);
  if (!ARRAY_APPEND(link->callbacks, link->n_callbacks, callbacks))
    return;

  if (callbacks.buf_lock) {
    callbacks.buf_lock->buf_list = capture;
  }
}

int camera_set_params(camera_t *camera)
{
  device_set_fps(camera->camera, camera->options.fps);
  device_set_option_list(camera->camera, camera->options.options);
  device_set_option_list(camera->isp, camera->options.isp.options);

  if (camera->options.auto_focus) {
    device_set_option_string(camera->camera, "AfTrigger", "1");
  }

  // Set some defaults
  device_set_option_list(camera->codec_snapshot, camera->options.snapshot.options);
  device_set_option_list(camera->codec_stream, camera->options.stream.options);
  device_set_option_string(camera->codec_video, "repeat_sequence_header", "1"); // required for force key support
  device_set_option_list(camera->codec_video, camera->options.video.options);
  return 0;
}

int camera_run(camera_t *camera)
{
  bool running = false;
  return links_loop(camera->links, camera->options.force_active, &running);
}
