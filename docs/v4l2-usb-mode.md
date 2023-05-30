# High-performance mode via direct decoding for USB

This script uses direct decoding or passthrough of MJPEG or H264 streams from UVC camera into
re-encoded stream provided over web-interface.

```bash
tools/usb_camera.sh -help
tools/usb_camera.sh -camera-format=MJPEG ...
```

Currently the H264 is consider rather broken:

```bash
tools/usb_camera.sh -camera-format=H264 ...
```
