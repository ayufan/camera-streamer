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
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>

#include "opts/log.h"

typedef struct {
	char buf[10];
} fourcc_string;

fourcc_string fourcc_to_string(unsigned format);
