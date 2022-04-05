#pragma once

#include <stdbool.h>

typedef struct options_s {
  const char *name;
  void *ptr;
  union {
    unsigned *value;
    unsigned *value_uint;
    bool *value_bool;
  };
  const char *format;
  const char *help;
  unsigned size;
} option_t;

#define lambda(return_type, function_body) \
({ \
      return_type __fn__ function_body \
          __fn__; \
})

#define OPTION_FORMAT_uint "%d"
#define OPTION_FORMAT_bool "%d"

#define DEFINE_OPTION(_section, _name, _type) \
  { \
    .name = #_section "-" #_name, \
    .value_##_type = &_section##_options._name, \
    .format = OPTION_FORMAT_##_type, \
    .size = sizeof(_section##_options._name), \
  }

#define DEFINE_OPTION_PTR(_section, _name, _format) \
  { \
    .name = #_section "-" #_name, \
    .ptr = _section##_options._name, \
    .format = _format, \
    .size = sizeof(_section##_options._name), \
  }

int parse_opts(option_t *options, int argc, char *argv[]);
