#include "opts.h"
#include "hw/v4l2.h"

#include <stddef.h>
#include <stdio.h>

static void print_help(option_t *options)
{
  for (int i = 0; options[i].name; i++) {
    option_t *option = &options[i];
    printf("%40s\t", option->name);

    if (option->value_string) {
      printf(option->format, option->value_string);
    } else if (option->value_mapping) {
      for (int j = 0; option->value_mapping[j].name; j++) {
        if (option->value_mapping[j].value == *option->value) {
          printf("%s - ", option->value_mapping[j].name);
          break;
        }
      }

      printf(option->format, *option->value);
    } else {
      printf(option->format, *option->value);
    }

    printf("\n");
  }
}

static int parse_opt(option_t *options, const char *key, const char *value)
{
  option_t *option = NULL;

  for (int i = 0; options[i].name; i++) {
    if (!strcmp(key, options[i].name)) {
      option = &options[i];
      break;
    }
  }

  if (!option) {
    return -EINVAL;
  }

  int ret = 0;
  if (option->value_string) {
    ret = sscanf(value, option->format, option->value_string);
  } else if (option->value_mapping) {
    for (int j = 0; option->value_mapping[j].name; j++) {
      if (!strcmp(option->value_mapping[j].name, value)) {
        *option->value_uint = option->value_mapping[j].value;
        return 1;
      }
    }

    ret = sscanf(value, option->format, option->value);
  } else {
    ret = sscanf(value, option->format, option->value);
  }
  return ret;
}

int parse_opts(option_t *options, int argc, char *argv[])
{
  int arg;

  for (arg = 1; arg < argc; arg += 2) {
    const char *key = argv[arg];
    if (!strcmp(key, "-help")) {
      print_help(options);
      return -1;
    }

    if (arg+1 == argc) {
      E_LOG_ERROR(NULL, "The %s is missing argument.", key);
    }

    if (key[0] != '-') {
      E_LOG_ERROR(NULL, "The '%s' is not option (should start with -).", key);
    }

    int ret = parse_opt(options, key+1, argv[arg+1]);
    if (ret <= 0) {
      E_LOG_ERROR(NULL, "Parsing '%s %s' returned '%d'.", key, argv[arg+1], ret);
    }
  }

  return 0;

error:
  return -1;
}
