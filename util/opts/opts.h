#pragma once

#include <stdbool.h>

typedef struct option_value_s {
  const char *name;
  unsigned value;
} option_value_t;

typedef struct options_s {
  const char *name;
  const char *field_name;
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

#define OPTION_VALUE_LIST_SEP ";"

#define OPTION_FORMAT_uint   "%u"
#define OPTION_FORMAT_hex    "%08x"
#define OPTION_FORMAT_bool   "%d"
#define OPTION_FORMAT_float  "%f"
#define OPTION_FORMAT_string "%s"
#define OPTION_FORMAT_list   "%s"

#define DEFINE_OPTION(_name, _section, _field, _type, _desc) \
  { \
    .name = _name ? _name : #_section "-" #_field, \
    .field_name = #_section "-" #_field, \
    .value_##_type = &_section##_options._field, \
    .format = OPTION_FORMAT_##_type, \
    .size = sizeof(_section##_options._field), \
    .description = _desc, \
  }

#define DEFINE_OPTION_DEFAULT(_name, _section, _field, _type, _default_value, _desc) \
  { \
    .name = _name ? _name : #_section "-" #_field, \
    .field_name = #_section "-" #_field, \
    .value_##_type = &_section##_options._field, \
    .format = OPTION_FORMAT_##_type, \
    .size = sizeof(_section##_options._field), \
    .default_value = _default_value, \
    .description = _desc, \
  }

#define DEFINE_OPTION_VALUES(_name, _section, _field, _value_mapping, _desc) \
  { \
    .name = _name ? _name : #_section "-" #_field, \
    .field_name = #_section "-" #_field, \
    .value_hex = &_section##_options._field, \
    .format = OPTION_FORMAT_hex, \
    .value_mapping = _value_mapping, \
    .size = sizeof(_section##_options._field), \
    .description = _desc, \
  }

#define DEFINE_OPTION_PTR(_name, _section, _field, _type, _desc) \
  { \
    .name = _name ? _name : #_section "-" #_field, \
    .field_name = #_section "-" #_field, \
    .value_##_type = _section##_options._field, \
    .format = OPTION_FORMAT_##_type, \
    .size = sizeof(_section##_options._field), \
    .description = _desc, \
  }

int parse_opts(option_t *options, int argc, char *argv[]);
