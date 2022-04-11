#include "libcamera.hh"

int libcamera_device_open(device_t *dev)
{
  dev->libcamera = new device_libcamera_t{};

  dev->libcamera->camera_manager = std::make_shared<libcamera::CameraManager>();
  int ret = dev->libcamera->camera_manager->start();
  if (ret < 0) {
    E_LOG_ERROR(dev, "Cannot start camera_manager.");
  }

  dev->libcamera->camera = dev->libcamera->camera_manager->get(dev->path);
  if (!dev->libcamera->camera) {
    if (dev->libcamera->camera_manager->cameras().size()) {
      for(auto const &camera : dev->libcamera->camera_manager->cameras()) {
        E_LOG_INFO(dev, "Available Camera: %s", camera->id().c_str());
      }
    } else {
      E_LOG_INFO(dev, "No available cameras");
    }
    E_LOG_ERROR(dev, "Camera `%s` was not found.", dev->path);
  }

  if (dev->libcamera->camera->acquire()) {
    E_LOG_ERROR(dev, "Failed to acquire `%s` camera.", dev->path);
  }

  printf("camera manager started, and camera was found: %s\n", dev->libcamera->camera->id().c_str());
  return 0;

error:
  return -1;
}

void libcamera_device_close(device_t *dev)
{
  if (dev->libcamera) {
    if (dev->libcamera->camera) {
      dev->libcamera->camera->stop();
      dev->libcamera->camera->release();
    }

    delete dev->libcamera;
    dev->libcamera = NULL;
  }
}

int libcamera_device_set_decoder_start(device_t *dev, bool do_on)
{
  return -1;
}

int libcamera_device_video_force_key(device_t *dev)
{
  return -1;
}

int libcamera_device_set_fps(device_t *dev, int desired_fps)
{
  return -1;
}

int libcamera_device_set_option(device_t *dev, const char *key, const char *value)
{
  return -1;
}
