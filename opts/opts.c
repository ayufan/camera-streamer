#include "opts.h"
#include "opts/log.h"

#include <stddef.h>
#include <stdio.h>
#include <limits.h>

static int option_handler_print(option_t *option, char *data);
static int option_handler_set(option_t *option, char *data);

static void print_help(option_t *options)
{
  for (int i = 0; options[i].name; i++) {
    option_t *option = &options[i];

    printf("%40s\t", option->name);

    if (option->value_list) {
      char *string = option->value_list;
      char *token;
      int tokens = 0;

      while (token = strsep(&string, OPTION_VALUE_LIST_SEP)) {
        if (tokens++ > 0)
          printf("\n%40s\t", "");
        printf("%s", token);
      }

      if (!tokens)
        printf("(none)");
    } else if (option->value_string) {
      printf(option->format, option->value_string);
    } else {
      if (option->value_mapping) {
        for (int j = 0; option->value_mapping[j].name; j++) {
          if (option->value_mapping[j].value == *option->value) {
            printf("%s - ", option->value_mapping[j].name);
            break;
          }
        }
      }

      unsigned mask = UINT_MAX >> ((sizeof(*option->value) - option->size) * 8);
      printf(option->format, *option->value & mask);
    }

    printf("\n");
  }
}

static int parse_opt(option_t *options, const char *key)
{
  option_t *option = NULL;
  const char *value = strchr(key, '=');

  for (int i = 0; options[i].name; i++) {
    if (value) {
      if (!strncmp(key, options[i].name, value - key)) {
        option = &options[i];
        value++; // ignore '='
        break;
      }
    } else {
      // require exact match
      if (!strcmp(key, options[i].name)) {
        option = &options[i];
        value = option->default_value;
        break;
      }
    }
  }

  LOG_DEBUG(NULL, "Parsing '%s'. Got value='%s', and option='%s'", key, value, option ? option->name : NULL);

  if (!option || !value) {
    return -EINVAL;
  }

  int ret = 0;
  if (option->value_list) {
    char *ptr = option->value_list;

    if (*ptr) {
      strcat(ptr, OPTION_VALUE_LIST_SEP);
      ptr += strlen(ptr);
    }

    ret = sscanf(value, option->format, ptr);
  } else if (option->value_string) {
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

  for (arg = 1; arg < argc; arg++) {
    const char *key = argv[arg];

    if (key[0] == '-') {
      key++;
      if (key[0] == '-')
        key++;
    } else {
      LOG_ERROR(NULL, "The '%s' is not option (should start with - or --).", key);
    }

    if (!strcmp(key, "help")) {
      print_help(options);
      return -1;
    }

    int ret = parse_opt(options, key);
    if (ret <= 0) {
      LOG_ERROR(NULL, "Parsing '%s' returned '%d'.", argv[arg], ret);
    }
  }

  return 0;

error:
  return -1;
}
