#include "control.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

int device_option_normalize_name(const char *in, char *outp)
{
  // The output is always shorter, so `outp=in`
  // colour_correction_matrix => colourcorrectionmatrix
  // Colour Correction Matrix => colourcorrectionmatrix
  // ColourCorrectionMatrix => colourcorrectionmatrix

  char *out = outp;

  while (*in) {
    if (isalnum(*in)) {
      *out++ = tolower(*in++);
    } else {
      in++;
    }
  }

  *out++ = 0;
  return out - outp;
}
