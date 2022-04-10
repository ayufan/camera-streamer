#include "v4l2.h"

fourcc_string fourcc_to_string(unsigned format)
{
  fourcc_string fourcc;
  char *ptr = fourcc.buf;
	*ptr++ = format & 0x7F;
	*ptr++ = (format >> 8) & 0x7F;
	*ptr++ = (format >> 16) & 0x7F;
	*ptr++ = (format >> 24) & 0x7F;
	if (format & ((unsigned)1 << 31)) {
		*ptr++ = '-';
		*ptr++ = 'B';
		*ptr++ = 'E';
		*ptr++ = '\0';
	} else {
		*ptr++ = '\0';
	}
  *ptr++ = 0;
	return fourcc;
}
