# Streaming

Camera Streamer exposes a number of streaming capabilities.

## HTTP web server

All streams are exposed over very simple HTTP server, providing different streams for different purposes:

- `http://<ip>:8080/` - index page
- `http://<ip>:8080/snapshot` - provide JPEG snapshot (works well everywhere)
- `http://<ip>:8080/stream` - provide MJPEG stream (works well everywhere)
- `http://<ip>:8080/video` - provide automated video.mp4 or video.hls stream depending on browser used
- `http://<ip>:8080/video.mp4` or `http://<ip>:8080/video.mkv` - provide remuxed `mkv` or `mp4` stream (uses `ffmpeg` to remux, works as of now only in Desktop Chrome and Safari)
- `http://<ip>:8080/webrtc` - provide WebRTC feed

## WebRTC support

The WebRTC is accessible via `http://<ip>:8080/webrtc` by default and is available when there's H264 output generated.

WebRTC support is implemented using awesome [libdatachannel](https://github.com/paullouisageneau/libdatachannel/) library.

The support will be compiled by default when doing `make`.

## RTSP server

The camera-streamer implements RTSP server via `live555`. Enable it with:

- adding `--rtsp-port`: will enable RTSP server on 8554
- adding `--rtsp-port=1111`: will enable RTSP server on custom port

The camera-streamer will expose single video stream:

- `rtsp://<ip>:8554/stream.h264` - the resolution is configured with `--camera-video.height`
