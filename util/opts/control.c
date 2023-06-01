#include "control.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

static bool isalnum_dot(char c)
{
  return isalnum(c) || c == '.';
}

int device_option_normalize_name(const char *in, char *outp)
{
  // The output is always shorter, so `outp=in`
  // colour_correction_matrix => colourcorrectionmatrix
  // Colour Correction Matrix => colourcorrectionmatrix
  // ColourCorrectionMatrix => colourcorrectionmatrix
  // Colour.Correction.Matrix => colour.correction.matrix

  char *out = outp;

  while (*in) {
    if (isalnum_dot(*in)) {
      *out++ = tolower(*in++);
    } else {
      in++;
    }
  }

  *out++ = 0;
  return out - outp - 1;
}

bool device_option_is_equal(const char *a, const char *b)
{
  // Similar to device_option_normalize_name
  // but ignore case sensitiveness

  while (*a || *b) {
    while (*a && !isalnum_dot(*a))
      a++;
    while (*b && !isalnum_dot(*b))
      b++;

    if (tolower(*a) != tolower(*b))
      return 0;

    a++;
    b++;
  }

  return 1;
}
