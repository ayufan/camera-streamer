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
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#define __FILENAME__ __FILE__

typedef struct log_options_s {
  bool debug;
  bool verbose;
	unsigned stats;
  char filter[256];
} log_options_t;

extern log_options_t log_options;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

bool filter_log(const char *filename);

// assumes that name is first item
#define dev_name(dev) (dev ? *(const char**)dev : "?")
#define LOG_ERROR(dev, _msg, ...)		do { fprintf(stderr, "%s: %s: " _msg "\n", __FILENAME__, dev_name(dev), ##__VA_ARGS__); goto error; } while(0)
#define LOG_PERROR(dev, _msg, ...)		do { fprintf(stderr, "%s: %s: " _msg "\n", __FILENAME__, dev_name(dev), ##__VA_ARGS__); exit(-1); } while(0)
#define LOG_INFO(dev, _msg, ...)		do { fprintf(stderr, "%s: %s: " _msg "\n", __FILENAME__, dev_name(dev), ##__VA_ARGS__); } while(0)
#define LOG_VERBOSE(dev, _msg, ...)	do { if (log_options.debug || log_options.verbose || filter_log(__FILENAME__)) { fprintf(stderr, "%s: %s: " _msg "\n", __FILENAME__, dev_name(dev), ##__VA_ARGS__); } } while(0)
#define LOG_DEBUG(dev, _msg, ...)		do { if (log_options.debug || filter_log(__FILENAME__)) { fprintf(stderr, "%s: %s: " _msg "\n", __FILENAME__, dev_name(dev), ##__VA_ARGS__); } } while(0)

#define CLOCK_FROM_PARAMS -1

uint64_t get_monotonic_time_us(struct timespec *ts, struct timeval *tv);
uint64_t get_time_us(clockid_t clock, struct timespec *ts, struct timeval *tv, int64_t delays_us);
int shrink_to_block(int size, int block);
char *trim(char *s);

int ioctl_retried(const char *name, int fd, int request, void *arg);

#define ERR_IOCTL(dev, _fd, _request, _value, _msg, ...) do { \
		int ret; \
		if ((ret = ioctl_retried(dev_name(dev), _fd, _request, _value)) < 0) { \
			LOG_ERROR(dev, "ioctl(ret=%d, errno=%d): " _msg, ret, errno, ##__VA_ARGS__); \
		} \
	} while(0)

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define ARRAY_APPEND(arr, n_arr, item) ((n_arr) < ARRAY_SIZE(arr) ? ((arr[n_arr++] = item), true) : false)
#define ARRAY_FOREACH(type, key, arr, n_arr) \
	for (type *key = &arr[0]; key < &arr[n_arr]; key++)
