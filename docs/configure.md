# Configure

## Resolution

Camera capture and resolution exposed is controlled by three parameters:

- `--camera-width` and `--camera-height` define the camera capture resolution
- `--camera-snapshot.height` - define height for an aspect ratio scaled resolution for `/snapshot` (JPEG) output - this might require rescaller and might not always work
- `--camera-video.height` - define height for an aspect ratio scaled resolution for `/video` and `/webrtc` (H264) output - this might require rescaller and might not always work, this is no larger than `--camera-snapshot.height`
- `--camera-stream.height` - define height for an aspect ratio scaled resolution for `/stream` (MJPEG) output - this might require rescaller and might not always work, this is no larger than `--camera-video.height`

The resolution scaling is following the `snapshot >= video >= stream`:

- `snapshot` provides a high quality image for the purpose of timelapses (3Mbps-10Mbps)
- `video` is an efficient high-quality H264 video stream (3-10Mbps)
- `stream` is an inefficient MJPEG stream requiring significant amount of bandwidth (10-100Mbps)

### Example: Raspberry PI v3 Camera (best)

```text
libcamera-still --list-cameras
Available cameras
-----------------
0 : imx708_wide [4608x2592] (/base/soc/i2c0mux/i2c@1/imx708@1a)
    Modes: 'SRGGB10_CSI2P' : 1536x864 [120.13 fps - (0, 0)/4608x2592 crop]
                             2304x1296 [56.03 fps - (0, 0)/4608x2592 crop]
                             4608x2592 [14.35 fps - (0, 0)/4608x2592 crop]
```

To get the best resolution for the above camera, the `--camera-width` and `--camera-height`
needs to be configured with the native resolution of camera sensor, only then the resolution
can be downscaled.

```text
--camera-width=2304 --camera-height=1296 --camera-snapshot.height=1080 --camera-video.height=720 --camera-stream.height=480
```

This will result in:

- camera will capture `2304x1296` a full sensor
- `snapshot` be ~1920x1080
- `video` be ~1280x720
- `stream` be ~640x480

### Example: Raspberry PI v3 Camera with cropped output (bad)

If the camera capture is not configured properly the result image will be cropped.

```text
--camera-width=1920 --camera-height=1080 --camera-snapshot.height=1080 --camera-video.height=720 --camera-stream.height=480
```

This will result in:

- camera will capture the middle `1920x1080` from the `2304x1296`
- `snapshot` be ~1920x1080
- `video` be ~1280x720
- `stream` be ~640x480

## List all available controls

You can view all available configuration parameters by adding `--log-verbose`
to one of the above commands, like:

```bash
tools/libcamera_camera.sh --camera-list_options ...
```

Or navigate to `http://<ip>:8080/option` while running.

Depending on control they have to be used for camera, ISP, or JPEG or H264 codec:

```bash
# specify camera option
--camera-options=brightness=1000

# specify ISP option
--camera-isp.options=digital_gain=1000

# specify H264 option
--camera-video.options=bitrate=10000000

# specify snapshot option
--camera-snapshot.options=compression_quality=60
--camera-stream.options=compression_quality=60
```

## List all available formats and use proper one

You might list all available capture formats for your camera:

```bash
v4l2-ctl -d /dev/video0 --list-formats-ext
```

Some of them might be specified to streamer:

```bash
tools/*_camera.sh --camera-format=RG10 # Bayer 10 packed
tools/*_camera.sh --camera-format=YUYV
tools/*_camera.sh --camera-format=MJPEG
tools/*_camera.sh --camera-format=H264 # This is unstable due to h264 key frames support
```

It is advised to always use:

- for `libcamera` the `--camera-type=libcamera --camera-format=YUYV` (better image quality) or `--camera-format=YUV420` (better performance)
- for `USB cameras` the `--camera-type=libcamera --camera-format=MJPEG`
