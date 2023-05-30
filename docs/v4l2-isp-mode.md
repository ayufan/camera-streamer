# High-performance mode via ISP for CSI

This script uses high-performance implementation for directly
accessing sensor feeds and passing it via DMA into bcm2385 ISP.
As such it does not implement brightness control similarly to `libcamera`.

```bash
# This script uses dumped IMX519 parametrs that are feed into bcm2385 ISP module
# This does not provide automatic brightness control
# Other sensors can be supported the same way as long as ISP parameters are adapted
tools/csi_camera.sh -help
tools/csi_camera.sh -camera-format=RG10 ...
```

This mode allows to provide significantly better performance for camera sensors
than described in specs. For example for Arducam 16MP (using IMX519) it is possible to achieve:

1. 120fps on 1280x720, with latency of about 50ms, where-as documentation says 1280x720x60p
1. 60fps on 1920x1080, with latency of about 90ms, where-as documentation says 1920x1080x30p
1. 30fps on 2328x1748, with latency of about 140ms, where-as documentation says 2328x1748x15p

The above was tested with camera connected to Raspberry PI Zero 2W, streaming `MJPEG` over WiFi
with camera recording https://www.youtube.com/watch?v=e8ZtPSIfWPc from desktop monitor. So, it as
well measured also a desktop rendering latency.
