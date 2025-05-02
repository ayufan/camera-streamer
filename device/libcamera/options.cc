#ifdef USE_LIBCAMERA
#include "libcamera.hh"
#include "third_party/magic_enum/include/magic_enum.hpp"

static std::string libcamera_device_option_normalize(std::string key)
{
  key.resize(device_option_normalize_name(key.data(), key.data()));
  return key;
}

template<typename T>
static std::map<unsigned, std::string> control_enum_values(const char *prefix)
{
  std::map<unsigned, std::string> ret;

  for (auto e : magic_enum::enum_entries<T>()) {
    auto value = std::string(e.second);
    if (prefix && value.find(prefix) == 0) {
      value = value.substr(strlen(prefix));
    }
    ret[e.first] = value;
  }

  return ret;
}

struct libcamera_control_id_t
{
  const libcamera::ControlId *control_id;
  std::map<unsigned, std::string> enum_values;
};

#define LIBCAMERA_CONTROL_RAW(Name, Strip) \
  { Name.id(), { .control_id = &Name, .enum_values = control_enum_values<Name##Enum>(Strip), } }

#define LIBCAMERA_CONTROL(Name, Strip) \
  LIBCAMERA_CONTROL_RAW(libcamera::controls::Name, Strip ? Strip : #Name)

#define LIBCAMERA_DRAFT_CONTROL(Name) \
  LIBCAMERA_CONTROL_RAW(libcamera::controls::draft::Name, #Name)

static std::map<unsigned, libcamera_control_id_t> libcamera_control_ids =
{
  LIBCAMERA_CONTROL(AeMeteringMode, "Metering"),
  LIBCAMERA_CONTROL(AeConstraintMode, "Constraint"),
  LIBCAMERA_CONTROL(AeExposureMode, "Exposure"),
  LIBCAMERA_CONTROL(AwbMode, "Awb"),
  LIBCAMERA_CONTROL(AfMode, "AfMode"),
  LIBCAMERA_CONTROL(AfRange, "AfRange"),
  LIBCAMERA_CONTROL(AfSpeed, "AfSpeed"),
  LIBCAMERA_CONTROL(AfTrigger, "AfTrigger"),
  LIBCAMERA_CONTROL(AfState, "AfState"),
#if LIBCAMERA_VERSION_MAJOR == 0 && LIBCAMERA_VERSION_MINOR >= 5 // Support for older libcamera versions
  LIBCAMERA_CONTROL(AeState, "AeState"),
#else
  LIBCAMERA_DRAFT_CONTROL(AeState),
#endif
  LIBCAMERA_DRAFT_CONTROL(AePrecaptureTrigger),
  LIBCAMERA_DRAFT_CONTROL(NoiseReductionMode),
  LIBCAMERA_DRAFT_CONTROL(ColorCorrectionAberrationMode),
  LIBCAMERA_DRAFT_CONTROL(AwbState),
  LIBCAMERA_DRAFT_CONTROL(LensShadingMapMode),
#if LIBCAMERA_VERSION_MAJOR == 0 && LIBCAMERA_VERSION_MINOR < 1 // Support RasPI bullseye
  LIBCAMERA_DRAFT_CONTROL(SceneFlicker),
#endif
  LIBCAMERA_DRAFT_CONTROL(TestPatternMode)
};

static std::set<const libcamera::ControlId *> ignored_controls =
{
  &libcamera::controls::FrameDurationLimits
};

static auto control_type_values = control_enum_values<libcamera::ControlType>("ControlType");

static const std::map<unsigned, std::string> *libcamera_find_control_ids(unsigned control_id)
{
  auto iter = libcamera_control_ids.find(control_id);
  if (iter == libcamera_control_ids.end())
    return NULL;

  return &iter->second.enum_values;
}

static long long libcamera_find_control_id_named_value(unsigned control_id, const char *name)
{
  auto named_values = libcamera_find_control_ids(control_id);
  if (named_values) {
    for (const auto & named_value : *named_values) {
      if (device_option_is_equal(named_value.second.c_str(), name))
        return named_value.first;
    }
  }

  return strtoll(name, NULL, 10);
}

static libcamera::ControlInfoMap::Map libcamera_control_list(device_t *dev)
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

    fprintf(stream, "- property: %s (%08x, type=%s) = %s\n",
      control_id_name.c_str(), control_id,
      control_type_values[control_value.type()].c_str(),
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

    fprintf(stream, "- available option: %s (%08x, type=%s): %s\n",
      control_id->name().c_str(), control_id->id(),
      control_type_values[control_id->type()].c_str(),
      control_info.toString().c_str());

    auto named_values = libcamera_find_control_ids(control_id->id());

    if (named_values != NULL) {
      for (const auto & named_value : *named_values) {
        fprintf(stream, "\t\t%d: %s\n", named_value.first, named_value.second.c_str());
      }
    } else {
      for (size_t i = 0; i < control_info.values().size(); i++) {
        fprintf(stream, "\t\t%s\n", control_info.values()[i].toString().c_str());
      }
    }
  }
  fprintf(stream, "\n");
}

static int libcamera_device_dump_control_option(device_option_fn fn, void *opaque, const libcamera::ControlId &control_id, const libcamera::ControlInfo *control_info, const libcamera::ControlValue *control_value, bool read_only)
{
  if (ignored_controls.find(&control_id) != ignored_controls.end()) {
    return 0;
  }

  device_option_t opt = {
    .control_id = control_id.id()
  };
  opt.flags.read_only = read_only;
  strcpy(opt.name, control_id.name().c_str());

  if (control_info) {
    strcpy(opt.description, control_info->toString().c_str());
  }

  if (control_value) {
    if (!control_value->isNone()) {
      strcpy(opt.value, control_value->toString().c_str());
    } else {
      control_value = NULL;
    }
  }

  switch (control_id.type()) {
  case libcamera::ControlTypeNone:
    break;

  case libcamera::ControlTypeBool:
    opt.type = device_option_type_bool;
    break;

  case libcamera::ControlTypeByte:
    opt.type = device_option_type_u8;
    break;

  case libcamera::ControlTypeInteger32:
    opt.type = device_option_type_integer;
    break;

  case libcamera::ControlTypeInteger64:
    opt.type = device_option_type_integer64;
    break;

  case libcamera::ControlTypeFloat:
    opt.type = device_option_type_float;
    break;

  case libcamera::ControlTypeRectangle:
    opt.type = device_option_type_float;
    opt.elems = 4;
    break;

  case libcamera::ControlTypeSize:
    opt.type = device_option_type_float;
    opt.elems = 2;
    break;

  case libcamera::ControlTypeString:
    opt.type = device_option_type_string;
    break;

#if LIBCAMERA_VERSION_MAJOR == 0 && LIBCAMERA_VERSION_MINOR > 3 && LIBCAMERA_VERSION_PATCH >= 2 // Support for older libcamera versions
  case libcamera::ControlTypePoint:
    opt.type = device_option_type_float;
    opt.elems = 2;
    break;
#endif

  default:
    throw std::runtime_error("ControlType unsupported or not implemented");
  }

  auto named_values = libcamera_find_control_ids(control_id.id());

  if (named_values != NULL) {
    for (const auto & named_value : *named_values) {
      if (opt.menu_items >= MAX_DEVICE_OPTION_MENU) {
        opt.flags.invalid = true;
        break;
      }

      device_option_menu_t *opt_menu = &opt.menu[opt.menu_items++];
      opt_menu->id = named_value.first;
      strcpy(opt_menu->name, named_value.second.c_str());
      if (control_value && atoi(control_value->toString().c_str()) == opt_menu->id) {
        strcpy(opt.value, opt_menu->name);
      }
    }
  } else if (control_info) {
    for (size_t i = 0; i < control_info->values().size(); i++) {
      if (opt.menu_items >= MAX_DEVICE_OPTION_MENU) {
        opt.flags.invalid = true;
        break;
      }

      device_option_menu_t *opt_menu = &opt.menu[opt.menu_items++];
      opt_menu->id = atoi(control_info->values()[i].toString().c_str());
      strcpy(opt_menu->name, control_info->values()[i].toString().c_str());
      if (control_value && atoi(control_value->toString().c_str()) == opt_menu->id) {
        strcpy(opt.value, opt_menu->name);
      }
    }
  } else if (control_value && control_value->numElements() > 0) {
    opt.elems = control_value->numElements();
  }

  if (opt.type) {
    int ret = fn(&opt, opaque);
    if (ret < 0)
      return ret;
  }

  return 0;
}

int libcamera_device_dump_options2(device_t *dev, device_option_fn fn, void *opaque)
{
  auto &properties = dev->libcamera->camera->properties();
  auto idMap = properties.idMap();

  int n = 0;

  for (auto const &control : properties) {
    if (!control.first)
      continue;

    auto control_id = control.first;
    auto control_value = control.second;
    std::string control_id_name = "";

    if (auto control_id_info = idMap ? idMap->at(control_id) : NULL) {
      int ret = libcamera_device_dump_control_option(fn, opaque, *control_id_info, NULL, &control_value, true);
      if (ret < 0)
        return ret;

      n++;
    }
  }

  for (auto const &control : libcamera_control_list(dev)) {
    if (!control.first)
      continue;

    auto control_id = control.first;
    auto control_info = control.second;

    auto control_value = dev->libcamera->applied_controls.contains(control_id->id())
      ? &dev->libcamera->applied_controls.get(control_id->id())
      : nullptr;

    int ret = libcamera_device_dump_control_option(fn, opaque,
      *control_id, &control_info, control_value, false);
    if (ret < 0)
      return ret;
    n++;
  }

  return n;
}

static libcamera::Rectangle libcamera_parse_rectangle(const char *value)
{
  static const char *RECTANGLE_PATTERNS[] =
  {
    "(%d,%d)/%ux%u",
    "%d,%d,%u,%u",
    NULL
  };

  for (int i = 0; RECTANGLE_PATTERNS[i]; i++) {
    libcamera::Rectangle rectangle;

    if (4 == sscanf(value, RECTANGLE_PATTERNS[i],
      &rectangle.x, &rectangle.y,
      &rectangle.width, &rectangle.height)) {
      return rectangle;
    }
  }

  return libcamera::Rectangle();
}

static libcamera::Size libcamera_parse_size(const char *value)
{
  static const char *SIZE_PATTERNS[] =
  {
    "%ux%u",
    "%u,%u",
    NULL
  };

  for (int i = 0; SIZE_PATTERNS[i]; i++) {
    libcamera::Size size;

    if (2 == sscanf(value, SIZE_PATTERNS[i], &size.width, &size.height)) {
      return size;
    }
  }

  return libcamera::Size();
}

#if LIBCAMERA_VERSION_MAJOR == 0 && LIBCAMERA_VERSION_MINOR > 3 && LIBCAMERA_VERSION_PATCH >= 2 // Support for older libcamera versions
static libcamera::Point libcamera_parse_point(const char *value)
{
  static const char *POINT_PATTERNS[] =
  {
    "(%d,%d)",
    "%d,%d",
    NULL
  };

  for (int i = 0; POINT_PATTERNS[i]; i++) {
    libcamera::Point point;

    if (2 == sscanf(value, POINT_PATTERNS[i],
      &point.x, &point.y)) {
      return point;
    }
  }

  return libcamera::Point();
}
#endif

template<typename T, typename F>
static bool libcamera_parse_control_value(libcamera::ControlValue &control_value, const char *value, const F &fn)
{
  std::vector<T> parsed;

  while (value && *value) {
    std::string current_value;

    if (const char *next = strchr(value, ',')) {
      current_value.assign(value, next);
      value = &next[1];
    } else {
      current_value.assign(value);
      value = NULL;
    }

    if (current_value.empty())
      continue;

    parsed.push_back(fn(current_value.c_str()));
  }

  if (parsed.empty()) {
    return false;
  }

  if (parsed.size() > 1) {
    control_value.set<libcamera::Span<T> >(parsed);
  } else {
    control_value.set<T>(parsed[0]);
  }

  return true;
}

int libcamera_device_set_option(device_t *dev, const char *keyp, const char *value)
{
  for (auto const &control : libcamera_control_list(dev)) {
    if (!control.first)
      continue;

    auto control_id = control.first;
    auto control_key = control_id->name();
    if (!device_option_is_equal(keyp, control_key.c_str()))
      continue;

    libcamera::ControlValue control_value;

    switch (control_id->type()) {
    case libcamera::ControlTypeNone:
      break;

    case libcamera::ControlTypeBool:
      control_value.set<bool>(atoi(value));
      break;

    case libcamera::ControlTypeByte:
      libcamera_parse_control_value<unsigned char>(control_value, value,
        [control_id](const char *value) { return libcamera_find_control_id_named_value(control_id->id(), value); });
      break;

    case libcamera::ControlTypeInteger32:
      libcamera_parse_control_value<int32_t>(control_value, value,
        [control_id](const char *value) { return libcamera_find_control_id_named_value(control_id->id(), value); });
      break;

    case libcamera::ControlTypeInteger64:
      libcamera_parse_control_value<int64_t>(control_value, value,
        [control_id](const char *value) { return libcamera_find_control_id_named_value(control_id->id(), value); });
      break;

    case libcamera::ControlTypeFloat:
      libcamera_parse_control_value<float>(control_value, value, atof);
      break;

    case libcamera::ControlTypeRectangle:
      libcamera_parse_control_value<libcamera::Rectangle>(
        control_value, value, libcamera_parse_rectangle);
      break;

    case libcamera::ControlTypeSize:
      libcamera_parse_control_value<libcamera::Size>(
        control_value, value, libcamera_parse_size);
      break;

    case libcamera::ControlTypeString:
      break;

#if LIBCAMERA_VERSION_MAJOR == 0 && LIBCAMERA_VERSION_MINOR > 3 && LIBCAMERA_VERSION_PATCH >= 2 // Support for older libcamera versions
    case libcamera::ControlTypePoint:
      libcamera_parse_control_value<libcamera::Point>(
        control_value, value, libcamera_parse_point);
      break;
#endif

    default:
      throw std::runtime_error("ControlType unsupported or not implemented");
    }

    if (control_value.isNone()) {
      LOG_ERROR(dev, "The `%s` type `%d` is not supported.", control_key.c_str(), control_id->type());
    }

    LOG_INFO(dev, "Configuring option '%s' (%08x, type=%d) = %s", 
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
