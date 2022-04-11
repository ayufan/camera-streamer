#include "control.h"

#include <ctype.h>
#include <stdbool.h>

int device_option_normalize_name(const char *in, char *outp)
{
  // The output is always shorter, so `outp=in`
  // colour_correction_matrix => ColourCorrectionMatrix
  // Colour Correction Matrix => ColourCorrectionMatrix
  // ColourCorrectionMatrix => ColourCorrectionMatrix

  char *out = outp;
  bool upper = true;

  while (*in) {
    if (isalnum(*in)) {
      if (upper) {
        *out++ = toupper(*in++);
        upper = false;
      } else {
        *out++ = *in++;
      }
    } else if (isprint(*in)) {
      upper = true;
      while (*++in && isprint(*in) && !isalnum(*in));
    } else {
      in++;
    }
  }

  *out++ = 0;
  return out - outp;
}
