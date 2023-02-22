#ifdef USE_LIBCAMERA
#include "libcamera.hh"

std::string libcamera_device_option_normalize(std::string key)
{
  key.resize(device_option_normalize_name(key.data(), key.data()));
  return key;
}

libcamera::ControlInfoMap::Map libcamera_control_list(device_t *dev)
{
  libcamera::ControlInfoMap::Map controls_map;
  for (auto const &control : dev->libcamera->camera->controls()) {
    controls_map[control.first] = control.second;
  }
  return controls_map;
}

void libcamera_device_dump_options(device_t *dev, FILE *stream)
{
  auto &properties = dev->libcamera->camera->properties();
  auto idMap = properties.idMap();

  fprintf(stream, "%s Properties:\n", dev->name);

  for (auto const &control : properties) {
    if (!control.first)
      continue;

    auto control_id = control.first;
    auto control_value = control.second;
    std::string control_id_name = "";

    if (auto control_id_info = idMap ? idMap->at(control_id) : NULL) {
      control_id_name = control_id_info->name();
    }

    fprintf(stream, "- property: %s (%08x, type=%d): %s\n",
      control_id_name.c_str(), control_id, control_value.type(),
      control_value.toString().c_str());
  }

  fprintf(stream, "\n");
  fprintf(stream, "%s Options:\n", dev->name);

  for (auto const &control : libcamera_control_list(dev)) {
    if (!control.first)
      continue;

    auto control_id = control.first;
    auto control_key = libcamera_device_option_normalize(control_id->name());
    auto control_info = control.second;

    fprintf(stream, "- available option: %s (%08x, type=%d): %s\n",
      control_id->name().c_str(), control_id->id(), control_id->type(),
      control_info.toString().c_str());
  }
  fprintf(stream, "\n");
}

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
  }

  if (!dev->libcamera->camera) {
    libcamera_print_cameras(dev);
    LOG_ERROR(dev, "Camera `%s` was not found.", dev->path);
  }

  if (dev->libcamera->camera->acquire()) {
    LOG_ERROR(dev, "Failed to acquire `%s` camera.", dev->libcamera->camera->id().c_str());
  }

  dev->libcamera->configuration = dev->libcamera->camera->generateConfiguration(
    { libcamera::StreamRole::Raw, libcamera::StreamRole::StillCapture });

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
  int64_t frame_time = 1000000 / desired_fps;
  dev->libcamera->controls.set(libcamera::controls::FrameDurationLimits, libcamera::Span<const int64_t, 2>({ frame_time, frame_time }));
  return 0;
}

int libcamera_device_set_rotation(device_t *dev, bool vflip, bool hflip)
{
  dev->libcamera->vflip = vflip;
  dev->libcamera->hflip = hflip;
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

    case libcamera::ControlTypeRectangle:
      static const char *RECTANGLE_PATTERNS[] = {
        "(%d,%d)/%ux%u",
        "%d,%d,%u,%u",
        NULL
      };

      for (int i = 0; RECTANGLE_PATTERNS[i]; i++) {
        libcamera::Rectangle rectangle;
      
        if (4 == sscanf(value, RECTANGLE_PATTERNS[i],
          &rectangle.x, &rectangle.y,
          &rectangle.width, &rectangle.height)) {
          control_value.set(rectangle);
          break;
        }
      }
      break;

    case libcamera::ControlTypeSize:
      static const char *SIZE_PATTERNS[] = {
        "%ux%u",
        "%u,%u",
        NULL
      };

      for (int i = 0; SIZE_PATTERNS[i]; i++) {
        libcamera::Size size;

        if (2 == sscanf(value, SIZE_PATTERNS[i], &size.width, &size.height)) {
          control_value.set(size);
          break;
        }
      }
      break;
  
    case libcamera::ControlTypeString:
      break;
    }

    if (control_value.isNone()) {
      LOG_ERROR(dev, "The `%s` type `%d` is not supported.", control_key.c_str(), control_id->type());
    }

    LOG_INFO(dev, "Configuring option %s (%08x, type=%d) = %s", 
      control_key.c_str(), control_id->id(), control_id->type(),
      control_value.toString().c_str());
    dev->libcamera->controls.set(control_id->id(), control_value);
    return 1;
  }

  return 0;

error:
  return -1;
}
#endif // USE_LIBCAMERA
