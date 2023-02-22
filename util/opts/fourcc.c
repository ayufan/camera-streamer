#include "fourcc.h"

#include <string.h>

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

many_fourcc_string many_fourcc_to_string(unsigned formats[])
{
	many_fourcc_string fourcc = {0};

	for (int i = 0; formats[i]; i++) {
		if (fourcc.buf[0]) {
			strcat(fourcc.buf, ", ");
		}
		strcat(fourcc.buf, fourcc_to_string(formats[i]).buf);
	}

	return fourcc;
}
