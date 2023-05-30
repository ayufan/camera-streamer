# Performance analysis

## Arducam 16MP

The 16MP sensor is supported by default in Raspberry PI OS after adding to `/boot/config.txt`.
However it will not support auto-focus nor manual focus due to lack of `ak7535` compiled
and enabled in `imx519`. Focus can be manually controlled via `i2c-tools`:

```shell
# /boot/config.txt
dtoverlay=imx519,media-controller=0
gpu_mem=160 # at least 128

# /etc/modules-load.d/modules.conf
i2c-dev

# after starting camera execute to control the focus with `0xXX`, any value between `0x00` to `0xff`
# RPI02W (and possible 2+, 3+):
i2ctransfer -y 22 w4@0x0c 0x0 0x85 0x00 0x00

# RPI4:
i2ctransfer -y 11 w4@0x0c 0x0 0xXX 0x00 0x00
```

Latency according to my tests is due to the way how buffers are enqueued and processing delay, this is for triple buffering in a case where sensor is able to deliver frames quick enough. Depending on how you queue (on which slope) and when you enqueue buffer you might achieve significantly better latency as shown in the ISP-mode. The `libcamera` can still achieve 120fps, it is just slightly slower :)

#### 2328x1748@30fps

```shell
# libcamera
$ ./camera_streamer -camera-path=/base/soc/i2c0mux/i2c@1/imx519@1a -camera-type=libcamera -camera-format=YUYV -camera-fps=120 -camera-width=2328 -camera-height=1748 -camera-high_res_factor=1.5 -log-filter=buffer_lock
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf2 (refs=2), frame=63/0, processing_ms=101.1, frame_ms=33.1
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf0 (refs=2), frame=64/0, processing_ms=99.2, frame_ms=31.9
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf1 (refs=2), frame=65/0, processing_ms=99.6, frame_ms=34.8

# direct ISP-mode
$ ./camera_streamer -camera-path=/dev/video0 -camera-format=RG10 -camera-fps=30 -camera-width=2328 -camera-height=1748 -camera-high_res_factor=1.5 -log-filter=buffer_lock
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf1 (refs=2), frame=32/0, processing_ms=49.7, frame_ms=33.3
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf2 (refs=2), frame=33/0, processing_ms=49.7, frame_ms=33.3
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf0 (refs=2), frame=34/0, processing_ms=49.7, frame_ms=33.4
```

#### 2328x1748@10fps

```shell
# libcamera
$ ./camera_streamer -camera-path=/base/soc/i2c0mux/i2c@1/imx519@1a -camera-type=libcamera -camera-format=YUYV -camera-fps=10 -camera-width=2328 -camera-height=1748 -camera-high_res_factor=1.5 -log-filter=buffer_lock
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf2 (refs=2), frame=585/0, processing_ms=155.3, frame_ms=100.0
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf0 (refs=2), frame=586/0, processing_ms=155.5, frame_ms=100.2

# direct ISP-mode
$ ./camera_streamer -camera-path=/dev/video0 -camera-format=RG10 -camera-fps=10 -camera-width=2328 -camera-height=1748 -camera-high_res_factor=1.5 -log-filter=buffer_lock
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf1 (refs=2), frame=260/0, processing_ms=57.5, frame_ms=99.7
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf2 (refs=2), frame=261/0, processing_ms=57.6, frame_ms=100.0
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf0 (refs=2), frame=262/0, processing_ms=58.0, frame_ms=100.4
```

#### 1280x720@120fps for Arducam 16MP

```shell
# libcamera
$ ./camera_streamer -camera-path=/base/soc/i2c0mux/i2c@1/imx519@1a -camera-type=libcamera -camera-format=YUYV -camera-fps=120 -camera-width=1280 -camera-height=720 -log-filter=buffer_lock
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf0 (refs=2), frame=139/0, processing_ms=20.1, frame_ms=7.9
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf1 (refs=2), frame=140/0, processing_ms=20.6, frame_ms=8.8
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf2 (refs=2), frame=141/0, processing_ms=19.8, frame_ms=8.1

# direct ISP-mode
$ ./camera_streamer -camera-path=/dev/video0 -camera-format=RG10 -camera-fps=120 -camera-width=1280 -camera-height=720 -log-filter=buffer_lock
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf0 (refs=2), frame=157/0, processing_ms=18.5, frame_ms=8.4
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf1 (refs=2), frame=158/0, processing_ms=18.5, frame_ms=8.3
device/buffer_lock.c: http_jpeg: Captured buffer JPEG:capture:mplane:buf2 (refs=2), frame=159/0, processing_ms=18.5, frame_ms=8.3
```
