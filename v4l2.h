#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/videodev2.h>

#ifndef CFG_XIOCTL_RETRIES
#	define CFG_XIOCTL_RETRIES 4
#endif
#define XIOCTL_RETRIES ((unsigned)(CFG_XIOCTL_RETRIES))

// assumes that name is first item
#define dev_name(dev) (dev ? *(const char**)dev : "?")
#define E_LOG_ERROR(dev, _msg, ...)		do { fprintf(stderr, "%s: " _msg "\n", dev_name(dev), ##__VA_ARGS__); goto error; } while(0)
#define E_LOG_PERROR(dev, _msg, ...)		do { fprintf(stderr, "%s: " _msg "\n", dev_name(dev), ##__VA_ARGS__); exit(-1); } while(0)
#define E_LOG_INFO(dev, _msg, ...)		fprintf(stderr, "%s: " _msg "\n", dev_name(dev), ##__VA_ARGS__)
#define E_LOG_VERBOSE(dev, _msg, ...)	fprintf(stderr, "%s: " _msg "\n", dev_name(dev), ##__VA_ARGS__)
#define E_LOG_DEBUG(dev, _msg, ...)		fprintf(stderr, "%s: " _msg "\n", dev_name(dev), ##__VA_ARGS__)

typedef struct {
	char buf[10];
} fourcc_string;

fourcc_string fourcc_to_string(unsigned format);
unsigned fourcc_to_stride(unsigned width, unsigned format);
int xioctl(int fd, int request, void *arg);

#define E_XIOCTL(dev, _fd, _request, _value, _msg, ...) do { \
		int ret; \
		if ((ret = xioctl(_fd, _request, _value)) < 0) { \
			E_LOG_ERROR(dev, "xioctl(ret=%d): " _msg, ret, ##__VA_ARGS__); \
		} \
	} while(0)
