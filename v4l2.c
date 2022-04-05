#include "v4l2.h"

int xioctl(const char *name, int fd, int request, void *arg)
{
	int retries = XIOCTL_RETRIES;
	int retval = -1;

	do {
		retval = ioctl(fd, request, arg);
	} while (
		retval
		&& retries--
		&& (
			errno == EINTR
			|| errno == EAGAIN
			|| errno == ETIMEDOUT
		)
	);

	// cppcheck-suppress knownConditionTrueFalse
	if (retval && retries <= 0) {
		E_LOG_PERROR(NULL, "%s: ioctl(%08x) retried %u times; giving up", name, request, XIOCTL_RETRIES);
	}
	return retval;
}

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

static size_t align_size(size_t size, size_t to)
{
	return ((size + (to - 1)) & ~(to - 1));
}

unsigned fourcc_to_stride(unsigned width, unsigned format)
{
	switch (format) {
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_RGB565:
			return align_size(width * 2, 32);

		case V4L2_PIX_FMT_YUV420:
			return align_size(width * 3 / 2, 32);

		case V4L2_PIX_FMT_RGB24:
			return align_size(width * 3, 32);

		case V4L2_PIX_FMT_SRGGB10P:
			return align_size(width * 5 / 4, 32);

		case V4L2_PIX_FMT_JPEG:
		case V4L2_PIX_FMT_H264:
			return 0;

		default:
			E_LOG_PERROR(NULL, "Unknown format: %s", fourcc_to_string(format).buf);
	}
}
