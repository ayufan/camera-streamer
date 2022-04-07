#include "opts/log.h"

#define _GNU_SOURCE
#include <string.h>

char *
strstrn(const char *s, const char *find, size_t len)
{
	char c, sc;
	if ((c = *find++) != 0) {
		len--;

		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while (sc != c);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

bool filter_log(const char *filename)
{
  if (!log_options.filter[0])
    return false;

  const char *ptr = log_options.filter;
  do {
    const char *next = strchr(ptr, ',');
    if (!next)
      next = ptr + strlen(ptr);

    if(strstrn(filename, ptr, next - ptr))
      return true;

		ptr = next;
  } while(*ptr++);

  return false;
}
