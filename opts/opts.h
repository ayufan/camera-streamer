#pragma once

#include <stdbool.h>

typedef struct option_value_s {
  const char *name;
  unsigned value;
} option_value_t;

typedef struct options_s {
  const char *name;
  char *value_string;
  union {
    unsigned *value;
    unsigned *value_uint;
    unsigned *value_hex;
    bool *value_bool;
    float *value_float;
  };
  const char *format;
  const char *help;
  option_value_t *value_mapping;
  unsigned size;
} option_t;

#define lambda(return_type, function_body) \
({ \
      return_type __fn__ function_body \
          __fn__; \
})

#define OPTION_FORMAT_uint   "%d"
#define OPTION_FORMAT_hex    "%08x"
#define OPTION_FORMAT_bool   "%d"
#define OPTION_FORMAT_float  "%.1f"
#define OPTION_FORMAT_string "%s"

#define DEFINE_OPTION(_section, _name, _type) \
  { \
    .name = #_section "-" #_name, \
    .value_##_type = &_section##_options._name, \
    .format = OPTION_FORMAT_##_type, \
    .size = sizeof(_section##_options._name), \
  }

#define DEFINE_OPTION_VALUES(_section, _name, _value_mapping) \
  { \
    .name = #_section "-" #_name, \
    .value_hex = &_section##_options._name, \
    .format = OPTION_FORMAT_hex, \
    .value_mapping = _value_mapping, \
    .size = sizeof(_section##_options._name), \
  }

#define DEFINE_OPTION_PTR(_section, _name, _type) \
  { \
    .name = #_section "-" #_name, \
    .value_##_type = _section##_options._name, \
    .format = OPTION_FORMAT_##_type, \
    .size = sizeof(_section##_options._name), \
  }

int parse_opts(option_t *options, int argc, char *argv[]);
