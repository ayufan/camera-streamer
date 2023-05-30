# High-compatibility via `libcamera` on Raspberry PI

This script uses `libcamera` to access camera and provide
manual and automatic brightness and exposure controls.
The settings for those are configurable via described below controls.

This due to extra overhead has worse latency than direct decoding
via ISP.

```bash
tools/libcamera_camera.sh -help
tools/libcamera_camera.sh -camera-format=YUYV
```
