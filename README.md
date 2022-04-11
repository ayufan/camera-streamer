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

## Compile

```bash
apt-get -y install libavformat-dev libavutil-dev libavcodec-dev libcamera-dev v4l-utils pkg-config build-essential
make
sudo make install
```

## Use it

There are three modes of operation implemented offering different
compatibility to performance.

### High-compatibility via `libcamera`

This script uses `libcamera` to access camera and provide
manual and automatic brightness and exposure controls.
The settings for those are configurable via described below controls.

This due to extra overhead has worse latency than direct decoding
via ISP.

```bash
./libcamera_camera.sh -help
./libcamera_camera.sh -camera-format=YUYV
```

### High-performance mode via ISP for CSI

This script uses high-performance implementation for directly
accessing sensor feeds and passing it via DMA into bcm2385 ISP.
As such it does not implement brightness control similarly to `libcamera`.

```bash
# This script uses dumped IMX519 parametrs that are feed into bcm2385 ISP module
# This does not provide automatic brightness control
# Other sensors can be supported the same way as long as ISP parameters are adapted
./imx519_camera.sh -help
./imx519_camera.sh -camera-format=RG10 ...
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
./usb_camera.sh -help
./usb_camera.sh -camera-format=H264 ...
./usb_camera.sh -camera-format=MJPEG ...
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

## List all available controls

You can view all available configuration parameters by adding `-log-verbose`
to one of the above commands, like:

```bash
./libcamera_camera.sh -log-verbose ...
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
./*_camera.sh -camera-format=RG10 # Bayer 10 packet
./*_camera.sh -camera-format=YUYV
./*_camera.sh -camera-format=MJPEG
./*_camera.sh -camera-format=H264 # This is unstable due to h264 key frames support
```

## License

GNU General Public License v3.0
