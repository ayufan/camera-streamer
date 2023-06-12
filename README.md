# camera-streamer

> Use `main` branch for semi-stable changes, or `develop` for experimental changes.

There's a number of great projects doing an UVC/CSI camera streaming
on SBC (like Raspberry PI's).

This is yet another **camera-streamer** project that is primarly focused
on supporting a fully hardware accelerated streaming of MJPEG streams
and H264 video streams for minimal latency.

This supports well CSI cameras that provide 10-bit Bayer packed format
from sensor, by using a dedicated ISP of Raspberry PI's.

Take into account that this is a draft project, and is nowhere as complete
and well supported as [awesome ustreamer](https://github.com/pikvm/ustreamer).
This project was inspired by mentioned ustreamer.

## Install

1. [Use precompiled debian package](https://github.com/ayufan/camera-streamer/releases/latest) (recommended)
2. [Compile manually](docs/install-manual.md) (advanced)

## Configure

1. [Configure resolution, brightness or image quality](docs/configure.md)
1. [See different streaming options](docs/streaming.md)
1. [See example configurations](service/)

## Advanced

This section contains some advanced explanations that are not complete and might be outdated:

1. [High-performance mode via ISP for CSI](docs/v4l2-isp-mode.md)
1. [High-performance mode via direct decoding for USB](docs/v4l2-usb-mode.md)
1. [High-compatibility via `libcamera` on Raspberry PI](docs/raspi-libcamera.md)
1. [Performance analysis](docs/performance-analysis.md)

## License

GNU General Public License v3.0
