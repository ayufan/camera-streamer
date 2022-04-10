#pragma once

// TODO: temporary
#include "device/hw/v4l2.h"

typedef struct buffer_s buffer_t;
typedef struct buffer_list_s buffer_list_t;
typedef struct device_s device_t;
struct pollfd;

int v4l2_device_open(device_t *dev);
void v4l2_device_close(device_t *dev);
int v4l2_device_set_decoder_start(device_t *dev, bool do_on);
int v4l2_device_video_force_key(device_t *dev);
int v4l2_device_set_fps(device_t *dev, int desired_fps);
int v4l2_device_set_option(device_t *dev, const char *key, const char *value);

int v4l2_buffer_open(buffer_t *buf);
void v4l2_buffer_close(buffer_t *buf);
int v4l2_buffer_enqueue(buffer_t *buf, const char *who);
int v4l2_buffer_list_dequeue(buffer_list_t *buf_list, buffer_t **bufp);
int v4l2_buffer_list_refresh_states(buffer_list_t *buf_list);
int v4l2_buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue);

int v4l2_buffer_list_set_format(buffer_list_t *buf_list, unsigned width, unsigned height, unsigned format, unsigned bytesperline);
int v4l2_buffer_list_set_buffers(buffer_list_t *buf_list, int nbufs);
int v4l2_buffer_list_set_stream(buffer_list_t *buf_list, bool do_on);

int v4l2_device_open_media_device(device_t *dev);
int v4l2_device_open_v4l2_subdev(device_t *dev, int subdev);
int v4l2_device_set_pad_format(device_t *dev, unsigned width, unsigned height, unsigned format);

#ifndef CFG_XIOCTL_RETRIES
#	define CFG_XIOCTL_RETRIES 4
#endif
#define XIOCTL_RETRIES ((unsigned)(CFG_XIOCTL_RETRIES))

unsigned fourcc_to_stride(unsigned width, unsigned format);
int xioctl(const char *name, int fd, int request, void *arg);

#define E_XIOCTL(dev, _fd, _request, _value, _msg, ...) do { \
		int ret; \
		if ((ret = xioctl(dev_name(dev), _fd, _request, _value)) < 0) { \
			E_LOG_ERROR(dev, "xioctl(ret=%d): " _msg, ret, ##__VA_ARGS__); \
		} \
	} while(0)
