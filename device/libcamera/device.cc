#ifdef USE_LIBCAMERA
#include "libcamera.hh"

std::string libcamera_device_option_normalize(std::string key)
{
  key.resize(device_option_normalize_name(key.data(), key.data()));
  return key;
}

libcamera::ControlInfoMap libcamera_control_list(device_t *dev)
{
  return dev->libcamera->camera->controls();
}

int libcamera_device_open(device_t *dev)
{
  dev->libcamera = new device_libcamera_t{};

  dev->libcamera->camera_manager = std::make_shared<libcamera::CameraManager>();
  int ret = dev->libcamera->camera_manager->start();
  if (ret < 0) {
    LOG_ERROR(dev, "Cannot start camera_manager.");
  }

  dev->libcamera->camera = dev->libcamera->camera_manager->get(dev->path);
  if (!dev->libcamera->camera) {
    if (dev->libcamera->camera_manager->cameras().size()) {
      for(auto const &camera : dev->libcamera->camera_manager->cameras()) {
        LOG_INFO(dev, "Available Camera: %s", camera->id().c_str());
      }
    } else {
      LOG_INFO(dev, "No available cameras");
    }
    LOG_ERROR(dev, "Camera `%s` was not found.", dev->path);
  }

  if (dev->libcamera->camera->acquire()) {
    LOG_ERROR(dev, "Failed to acquire `%s` camera.", dev->path);
  }

	LOG_INFO(dev, "Device path=%s opened", dev->path);

  for (auto const &control : libcamera_control_list(dev)) {
    if (!control.first)
      continue;

    auto control_id = control.first;
    auto control_key = libcamera_device_option_normalize(control_id->name());

    LOG_VERBOSE(dev, "Available control: %s (%08x, type=%d)",
      control_key.c_str(), control_id->id(), control_id->type());
  }
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
  return 0;
}

int libcamera_device_set_option(device_t *dev, const char *keyp, const char *value)
{
  auto key = libcamera_device_option_normalize(keyp);

  for (auto const &control : libcamera_control_list(dev)) {
    if (!control.first)
      continue;

    auto control_id = control.first;
    auto control_key = libcamera_device_option_normalize(control_id->name());
    if (key != control_key)
      continue;

    libcamera::ControlValue control_value;

    switch (control_id->type()) {
    case libcamera::ControlTypeNone:
      break;

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
      LOG_ERROR(dev, "The `%s` type `%d` is not supported.", control_key.c_str(), control_id->type());
    }

    LOG_INFO(dev, "Configuring option %s (%08x, type=%d) = %s", 
      control_key.c_str(), control_id->id(), control_id->type(),
      control_value.toString().c_str());
    dev->libcamera->controls.set(control_id->id(), control_value);
    return 0;
  }

error:
  return -1;
}
#endif // USE_LIBCAMERA
