#pragma once

#include <time.h>
#include <linux/videodev2.h>

typedef struct {
	char buf[10];
} fourcc_string;

typedef struct {
	char buf[250];
} many_fourcc_string;

fourcc_string fourcc_to_string(unsigned format);
many_fourcc_string many_fourcc_to_string(unsigned formats[]);
