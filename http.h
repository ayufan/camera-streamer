#pragma once

#include "v4l2.h"

int http_listen(int listen_port, int maxcons);
int http_worker(int listenfd);
int http_worker_threads(int listenfd, int nthreads);
