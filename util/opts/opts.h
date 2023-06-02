#pragma once

#include <stdbool.h>

typedef struct option_value_s {
  const char *name;
  unsigned value;
} option_value_t;

typedef struct options_s {
  const char *name;
  char *value_string;
  char *value_list;
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
  const char *default_value;
  const char *description;
} option_t;

#define OPTION_VALUE_LIST_SEP_CHAR ';'
#define OPTION_VALUE_LIST_SEP ";"

#define OPTION_FORMAT_uint   "%u"
#define OPTION_FORMAT_hex    "%08x"
#define OPTION_FORMAT_bool   "%d"
#define OPTION_FORMAT_float  "%f"
#define OPTION_FORMAT_string "%s"
#define OPTION_FORMAT_list   "%s"

#define DEFINE_OPTION(_section, _name, _type, _desc) \
  { \
    .name = #_section "-" #_name, \
    .value_##_type = &_section##_options._name, \
    .format = OPTION_FORMAT_##_type, \
    .size = sizeof(_section##_options._name), \
    .description = _desc, \
  }

#define DEFINE_OPTION_DEFAULT(_section, _name, _type, _default_value, _desc) \
  { \
    .name = #_section "-" #_name, \
    .value_##_type = &_section##_options._name, \
    .format = OPTION_FORMAT_##_type, \
    .size = sizeof(_section##_options._name), \
    .default_value = _default_value, \
    .description = _desc, \
  }

#define DEFINE_OPTION_VALUES(_section, _name, _value_mapping, _desc) \
  { \
    .name = #_section "-" #_name, \
    .value_hex = &_section##_options._name, \
    .format = OPTION_FORMAT_hex, \
    .value_mapping = _value_mapping, \
    .size = sizeof(_section##_options._name), \
    .description = _desc, \
  }

#define DEFINE_OPTION_PTR(_section, _name, _type, _desc) \
  { \
    .name = #_section "-" #_name, \
    .value_##_type = _section##_options._name, \
    .format = OPTION_FORMAT_##_type, \
    .size = sizeof(_section##_options._name), \
    .description = _desc, \
  }

const char *opt_value_to_string(const option_value_t *values, int value, const char *def);
int opt_string_to_value(const option_value_t *values, const char *name, int def);

int parse_opts(option_t *options, int argc, char *argv[]);
