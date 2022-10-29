#include "device/buffer_lock.h"

DEFINE_BUFFER_LOCK(snapshot_lock, 1000);
DEFINE_BUFFER_LOCK(stream_lock, 0);
DEFINE_BUFFER_LOCK(video_lock, 0);
