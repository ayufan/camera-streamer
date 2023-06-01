#ifdef USE_LIBCAMERA
#include "libcamera.hh"

void libcamera_print_cameras(device_t *dev)
{
  if (dev->libcamera->camera_manager->cameras().size()) {
    LOG_INFO(dev, "Available cameras (%zu)", dev->libcamera->camera_manager->cameras().size());

    for (auto const &camera : dev->libcamera->camera_manager->cameras()) {
      LOG_INFO(dev, "- %s", camera->id().c_str());
    }
  } else {
    LOG_INFO(dev, "No available cameras");
  }
}

int libcamera_device_open(device_t *dev)
{
  dev->libcamera = new device_libcamera_t{};

  dev->libcamera->camera_manager = std::make_shared<libcamera::CameraManager>();
  int ret = dev->libcamera->camera_manager->start();
  if (ret < 0) {
    LOG_ERROR(dev, "Cannot start camera_manager.");
  }

  if (strlen(dev->path) == 0) {
    if (dev->libcamera->camera_manager->cameras().size() != 1) {
      libcamera_print_cameras(dev);
      LOG_ERROR(dev, "Too many cameras was found. Cannot select default.");
    }

    dev->libcamera->camera = dev->libcamera->camera_manager->cameras().front();
  } else {
    dev->libcamera->camera = dev->libcamera->camera_manager->get(dev->path);

    // Do partial match of camera
    if (!dev->libcamera->camera) {
      for (auto& camera : dev->libcamera->camera_manager->cameras()) {
        if (!strstr(camera->id().c_str(), dev->path))
          continue;

        if (dev->libcamera->camera) {
          libcamera_print_cameras(dev);
          LOG_ERROR(dev, "Many cameras matching found.");
        }
        dev->libcamera->camera = camera;
      }
    }
  }

  if (!dev->libcamera->camera) {
    libcamera_print_cameras(dev);
    LOG_ERROR(dev, "Camera `%s` was not found.", dev->path);
  }

  if (dev->libcamera->camera->acquire()) {
    LOG_ERROR(dev, "Failed to acquire `%s` camera.", dev->libcamera->camera->id().c_str());
  }

  dev->libcamera->configuration = dev->libcamera->camera->generateConfiguration();

  dev->libcamera->allocator = std::make_shared<libcamera::FrameBufferAllocator>(
    dev->libcamera->camera);

	LOG_INFO(dev, "Device path=%s opened", dev->libcamera->camera->id().c_str());
  return 0;

error:
  return -1;
}

void libcamera_device_close(device_t *dev)
{
  if (dev->libcamera) {
    if (dev->libcamera->camera) {
      dev->libcamera->camera->release();
    }

    delete dev->libcamera;
    dev->libcamera = NULL;
  }
}

int libcamera_device_set_fps(device_t *dev, int desired_fps)
{
  int64_t frame_time = desired_fps ? 1000000 / desired_fps : 0;
  dev->libcamera->controls.set(libcamera::controls::FrameDurationLimits, libcamera::Span<const int64_t, 2>({ frame_time, frame_time }));
  return 0;
}

int libcamera_device_set_rotation(device_t *dev, bool vflip, bool hflip)
{
  dev->libcamera->vflip = vflip;
  dev->libcamera->hflip = hflip;
  return 0;
}

void libcamera_device_apply_controls(device_t *dev)
{
  auto &controls = dev->libcamera->controls;
  auto &applied_controls = dev->libcamera->applied_controls;

  for (auto &control : controls) {
    applied_controls.set(control.first, control.second);
  }

  controls.clear();
}
#endif // USE_LIBCAMERA
