#include "opts.h"
#include "hw/v4l2.h"

#include <stddef.h>
#include <stdio.h>

static void print_help(option_t *options)
{
  for (int i = 0; options[i].name; i++) {
    printf("%20s ", options[i].name);

    if (options[i].ptr) {
      printf(options[i].format, options[i].ptr);
    } else {
      printf(options[i].format, *options[i].value);
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
  if (option->ptr) {
    ret = sscanf(value, option->format, option->ptr);
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
