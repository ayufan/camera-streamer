# Yet another camera streamer

There's a number of great projects doing an UVC/CSI camera streaming
on SBC (like Raspberry PI's).

This is yet another camera streamer project that is primarly focused
on supporting a fully hardware accelerated streaming of MJPEG streams
and H264 video streams for minimal latency.

This supports well CSI cameras that provide 10-bit Bayer packed format
from sensor, by using a dedicated ISP of Raspberry PI's.

Take into account that this is a draft project, and is nowhere as complete
and well supported as (awesome ustreamer)[https://github.com/pikvm/ustreamer].
This project was inspired by mentioned ustreamer.

## Validate your system

**This streamer does only use hardware, and does not support any software decoding or encoding**.
It does require system to provide:

1. ISP (`/dev/video13`, `/dev/video14`, `/dev/video15`)
2. JPEG encoder (`/dev/video31`)
3. H264 encoder (`/dev/video11`)
4. JPEG/H264 decoder (for UVC cameras, `/dev/video10`)
5. At least LTS kernel (5.15) for Raspberry PIs

You can validate the presence of all those devices with:

```bash
uname -a
v4l2-ctl --list-devices
```

The `5.15 kernel` is easy to get since this is LTS kernel for Raspberry PI OS:

```bash
apt-get update
apt-get dist-upgrade
reboot
```

## Compile

```bash
git clone https://github.com/ayufan-research/camera-streamer.git --recursive
apt-get -y install libavformat-dev libavutil-dev libavcodec-dev libcamera-dev liblivemedia-dev v4l-utils pkg-config xxd build-essential cmake libssl-dev

cd camera-streamer/
make
sudo make install
```

## Use it

There are three modes of operation implemented offering different
compatibility to performance.

### Use a preconfigured systemd services

The simplest is to use preconfigured `service/camera-streamer*.service`.
Those can be used as an example, and can be configured to fine tune parameters.

Example:

```bash
systemctl enable $PWD/service/camera-streamer-arducam-16MP.service
systemctl start camera-streamer-arducam-16MP
```

If everything was OK, there will be web-server at `http://<IP>:8080/`.

Error messages can be read `journalctl -xef -u camera-streamer-arducam-16MP`.

### High-compatibility via `libcamera`

This script uses `libcamera` to access camera and provide
manual and automatic brightness and exposure controls.
The settings for those are configurable via described below controls.

This due to extra overhead has worse latency than direct decoding
via ISP.

```bash
tools/libcamera_camera.sh -help
tools/libcamera_camera.sh -camera-format=YUYV
```

### High-performance mode via ISP for CSI

This script uses high-performance implementation for directly
accessing sensor feeds and passing it via DMA into bcm2385 ISP.
As such it does not implement brightness control similarly to `libcamera`.

```bash
# This script uses dumped IMX519 parametrs that are feed into bcm2385 ISP module
# This does not provide automatic brightness control
# Other sensors can be supported the same way as long as ISP parameters are adapted
tools/imx519_camera.sh -help
tools/imx519_camera.sh -camera-format=RG10 ...
```

This mode allows to provide significantly better performance for camera sensors
than described in specs. For example for Arducam 16MP (using IMX519) it is possible to achieve:

1. 120fps on 1280x720, with latency of about 50ms, where-as documentation says 1280x720x60p
1. 60fps on 1920x1080, with latency of about 90ms, where-as documentation says 1920x1080x30p
1. 30fps on 2328x1748, with latency of about 140ms, where-as documentation says 2328x1748x15p

The above was tested with camera connected to Raspberry PI Zero 2W, streaming `MJPEG` over WiFi
with camera recording https://www.youtube.com/watch?v=e8ZtPSIfWPc from desktop monitor. So, it as
well measured also a desktop rendering latency.

### High-performance mode via direct decoding for USB

This script uses direct decoding or passthrough of MJPEG or H264 streams from UVC camera into
re-encoded stream provided over web-interface.

```bash
tools/usb_camera.sh -help
tools/usb_camera.sh -camera-format=H264 ...
tools/usb_camera.sh -camera-format=MJPEG ...
```

## HTTP web server

All streams are exposed over very simple HTTP server, providing different streams for different purposes:

- `http://<ip>:8080/` - index page
- `http://<ip>:8080/snapshot` - provide JPEG snapshot (works well everywhere)
- `http://<ip>:8080/stream` - provide M-JPEG stream (works well everywhere)
- `http://<ip>:8080/video` - provide H264 in-browser muxed video output (using https://github.com/samirkumardas/jmuxer/, works only in Desktop Chrome)
- `http://<ip>:8080/video.mp4` or `http://<ip>:8080/video.mkv` - provide remuxed `mkv` or `mp4` stream (uses `ffmpeg` to remux, works as of now only in Desktop Chrome and Safari)

### Resolution

Camera capture and resolution exposed is controlled by threee parameters:

- `-camera-width` and `-camera-height` define capture resolution
- (ISP mode only) `-camera-high_res_factor` a default resolution exposed via HTTP (`exposed_width = camera_width / factor, exposed_height = camera_height / factor`)
- (ISP mode only) `-camera-low_res_factor` a low-resolution exposed via HTTP when `?res=low` is added (ex. `http://<ip>:8080/snapshot`)

## RTSP server

The camera-streamer implements RTSP server via `live555`. Enable it with:

- adding `-rtsp-port`: will enable RTSP server on 8554
- adding `-rtsp-port=1111`: will enable RTSP server on custom port

The camera-streamer will expose two stream (if low res mode is enabled):

- `rtsp://<ip>:8554/stream.h264` - high resolution stream (always enabled if H264 stream is available directly or via encoding)
- `rtsp://<ip>:8554/stream_low_res.h264` - low resolution stream if low res mode is configured via `-camera-low_res_factor`

## List all available controls

You can view all available configuration parameters by adding `-log-verbose`
to one of the above commands, like:

```bash
tools/libcamera_camera.sh -log-verbose ...
```

Depending on control they have to be used for camera, ISP, or JPEG or H264 codec:

```bash
# specify camera option
-camera-options=brightness=1000

# specify ISP option
-camera-isp.options=digital_gain=1000

# specify H264 option
-camera-h264.options=bitrate=10000000

# specify JPEG option
-camera-jpeg.options=compression_quality=60
```

## List all available formats and use proper one

You might list all available capture formats for your camera:

```bash
v4l2-ctl -d /dev/video0 --list-formats-ext
```

Some of them might be specified to streamer:

```bash
tools/*_camera.sh -camera-format=RG10 # Bayer 10 packet
tools/*_camera.sh -camera-format=YUYV
tools/*_camera.sh -camera-format=MJPEG
tools/*_camera.sh -camera-format=H264 # This is unstable due to h264 key frames support
```

## Camera support

### Arducam 16MP

The 16MP sensor is supported by default in Raspberry PI OS after adding to `/boot/config.txt`.
However it will not support auto-focus nor manual focus due to lack of `ak7535` compiled
and enabled in `imx519`. Focus can be manually controlled via `i2c-tools`:

```shell
# /boot/config.txt
dtoverlay=imx519,media-controller=0

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

## WebRTC support

The WebRTC is accessible via `http://<ip>:8080/video` by default and is available when there's H264 output generated.

WebRTC support is implemented using awesome [libdatachannel](https://github.com/paullouisageneau/libdatachannel/) library.

The support will be compiled by default when doing `make`.

## License

GNU General Public License v3.0
