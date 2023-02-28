#include "opts.h"
#include "version.h"
#include "util/opts/log.h"

#include <stddef.h>
#include <stdio.h>
#include <limits.h>

#define OPT_LENGTH 30

static void print_version(const char *cmd)
{
  printf("%s (%s)\n", GIT_VERSION, GIT_REVISION);
}

static void print_help(option_t *options, const char *cmd)
{
  printf("Usage:\n");
  printf("$ %s <options...>\n", cmd);
  printf("\n");

  printf("Options:\n");
  for (int i = 0; options[i].name; i++) {
    option_t *option = &options[i];
    int len = 0;

    len += printf("  --%s", option->name);
    if (option->default_value) {
      len += printf("[=%s]", option->default_value);
    } else if (option->value_mapping) {
      len += printf("=arg");
    } else {
      len += printf("=%s", option->format);
    }
    if (len < OPT_LENGTH) {
      printf("%*s", OPT_LENGTH-len, " ");
    }

    printf("- %s", option->description);
    if (option->value_mapping) {
      printf(" Values: ");
      for (int j = 0; option->value_mapping[j].name; j++) {
        if (j > 0) printf(", ");
        printf("%s", option->value_mapping[j].name);
      }
      printf(".");
    }
    printf("\n");
  }
  printf("\n");

  printf("Command line:\n\n");

  printf("$ %s \\\n", cmd);
  for (int i = 0; options[i].name; i++) {
    option_t *option = &options[i];
    int len = 0;

    if (option->value_list) {
      char *string = option->value_list;
      char *token;

      if (!*string)
        continue;

      while ((token = strsep(&string, OPTION_VALUE_LIST_SEP)) != NULL) {
        len = printf("  --%s=", option->name);
        printf("%s \\\n", token);
      }
    } else if (option->value_string) {
      len += printf("  --%s=", option->name);
      printf(option->format, option->value_string);
      printf(" \\\n");
    } else {
      bool found = false;
      len += printf("  --%s=", option->name);
      if (option->value_mapping) {
        for (int j = 0; option->value_mapping[j].name; j++) {
          if (option->value_mapping[j].value == *option->value_uint) {
            printf("%s", option->value_mapping[j].name);
            found = true;
            break;
          }
        }
      }

      if (!found) {
        unsigned mask = UINT_MAX >> ((sizeof(*option->value_uint) - option->size) * 8);
        printf(option->format, *option->value_uint & mask);
      }
      printf(" \\\n");
    }
  }
  printf("\n");
}

static int parse_opt(option_t *options, const char *key, int dash)
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

  if (dash == 2 && strlen(key) == 1) {
    LOG_INFO(NULL, "Usage of '--%s' is deprecated change to '-%s'.", key, key);
  } else if (dash == 1 && strlen(key) > 1) {
    LOG_INFO(NULL, "Usage of '-%s' is deprecated change to '--%s'.", key, key);
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
      if (key[0] == '-') {
        key++;
      }
    } else {
      LOG_ERROR(NULL, "The '%s' is not option (should start with - or --).", key);
    }

    if (!strcmp(key, "help")) {
      print_help(options, argv[0]);
      exit(-1);
      return -1;
    }

    if (!strcmp(key, "version")) {
      print_version(argv[0]);
      exit(0);
      return 0;
    }

    int ret = parse_opt(options, key, key - argv[arg]);
    if (ret <= 0) {
      LOG_ERROR(NULL, "Parsing '%s' returned '%d'.", argv[arg], ret);
    }
  }

  return 0;

error:
  return -1;
}
