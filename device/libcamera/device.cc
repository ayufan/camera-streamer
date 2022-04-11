#ifdef USE_LIBCAMERA
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
      dev->libcamera->camera->release();
    }

    delete dev->libcamera;
    dev->libcamera = NULL;
  }
}

int libcamera_device_set_fps(device_t *dev, int desired_fps)
{
  int64_t frame_time = 1000000 / desired_fps;
  dev->libcamera->controls.set(libcamera::controls::FrameDurationLimits, { frame_time, frame_time });
  return -1;
}

std::string libcamera_device_option_normalize(std::string key)
{
  key.resize(device_option_normalize_name(key.data(), key.data()));
  return key;
}

int libcamera_device_set_option(device_t *dev, const char *keyp, const char *value)
{
  auto key = libcamera_device_option_normalize(keyp);

  for (auto const &control : dev->libcamera->camera->controls()) {
    if (!control.first)
      continue;

    auto control_id = control.first;
    auto control_key = libcamera_device_option_normalize(control_id->name());
    if (key != control_key)
      continue;

    libcamera::ControlValue control_value;

    switch (control_id->type()) {
    case libcamera::ControlTypeBool:
      control_value.set<bool>(atoi(value));
      break;

    case libcamera::ControlTypeByte:
      control_value.set<unsigned char>(atoi(value));
      break;

    case libcamera::ControlTypeInteger32:
      control_value.set<int32_t>(atoi(value));
      break;

    case libcamera::ControlTypeInteger64:
      control_value.set<int64_t>(atoi(value));
      break;

    case libcamera::ControlTypeFloat:
      control_value.set<float>(atof(value));
      break;

    case libcamera::ControlTypeString:
    case libcamera::ControlTypeRectangle:
    case libcamera::ControlTypeSize:
      break;
    }

    if (control_value.isNone()) {
      E_LOG_ERROR(dev, "The `%s` type `%d` is not supported.", control_key.c_str(), control_id->type());
    }

    E_LOG_INFO(dev, "Configuring option %s (%08x, type=%d) = %s", 
      control_key.c_str(), control_id->id(), control_id->type(),
      control_value.toString().c_str());
    dev->libcamera->controls.set(control_id->id(), control_value);
    return 0;
  }

error:
  return -1;
}
#endif // USE_LIBCAMERA
