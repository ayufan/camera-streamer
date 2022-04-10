#pragma once

#include <linux/videodev2.h>

typedef struct {
	char buf[10];
} fourcc_string;

fourcc_string fourcc_to_string(unsigned format);
