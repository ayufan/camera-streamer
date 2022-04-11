#pragma once

extern "C" {
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "device/device.h"
#include "device/buffer_list.h"
#include "device/buffer.h"
#include "opts/log.h"
#include "opts/fourcc.h"
#include "opts/control.h"
};

#ifdef USE_LIBCAMERA
#include <optional>
#include <memory>

#include <libcamera/controls.h>
#include <libcamera/control_ids.h>
#include <libcamera/property_ids.h>
#include <libcamera/libcamera.h>
#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/request.h>
#include <libcamera/stream.h>
#include <libcamera/formats.h>
#include <libcamera/transform.h>

typedef struct buffer_s buffer_t;
typedef struct buffer_list_s buffer_list_t;
typedef struct device_s device_t;
struct pollfd;

typedef struct device_libcamera_s {
  std::shared_ptr<libcamera::CameraManager> camera_manager;
  std::shared_ptr<libcamera::Camera> camera;
  libcamera::ControlList controls;
} device_libcamera_t;

typedef struct buffer_list_libcamera_s {
  std::shared_ptr<libcamera::CameraConfiguration> configuration;
  std::shared_ptr<libcamera::FrameBufferAllocator> allocator;
  buffer_list_t *buf_list;
  int fds[2];

  void libcamera_buffer_list_dequeued(libcamera::Request *request);
} buffer_list_libcamera_t;

typedef struct buffer_libcamera_s {
  std::shared_ptr<libcamera::Request> request;
} buffer_libcamera_t;

int libcamera_device_open(device_t *dev);
void libcamera_device_close(device_t *dev);
int libcamera_device_set_fps(device_t *dev, int desired_fps);
int libcamera_device_set_option(device_t *dev, const char *key, const char *value);

int libcamera_buffer_open(buffer_t *buf);
void libcamera_buffer_close(buffer_t *buf);
int libcamera_buffer_enqueue(buffer_t *buf, const char *who);
void libcamera_buffer_list_dequeued(buffer_list_t *buf_list, libcamera::Request *request);
int libcamera_buffer_list_dequeue(buffer_list_t *buf_list, buffer_t **bufp);
int libcamera_buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue);

int libcamera_buffer_list_open(buffer_list_t *buf_list, const char *path, unsigned width, unsigned height, unsigned format, unsigned bytesperline, int nbufs);
void libcamera_buffer_list_close(buffer_list_t *buf_list);
int libcamera_buffer_list_set_stream(buffer_list_t *buf_list, bool do_on);
#endif // USE_LIBCAMERA
