#include "util/http/http.h"
#include "util/opts/opts.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"
#include "device/camera/camera.h"
#include "output/rtsp/rtsp.h"
#include "output/output.h"

camera_options_t camera_options = {
  .path = "",
  .width = 1920,
  .height = 1080,
  .format = 0,
  .nbufs = 3,
  .fps = 30,
  .allow_dma = true,
  .high_res_factor = 0.0,
  .low_res_factor = 0.0,
  .auto_reconnect = 0,
  .auto_focus = true,
  .options = "",
  .list_options = false,
  .snapshot = {
    .options = "compression_quality=80"
  },
  .stream = {
    .options = "compression_quality=80"
  },
  .video = {
    .options =
      "video_bitrate_mode=0" OPTION_VALUE_LIST_SEP
      "video_bitrate=2000000" OPTION_VALUE_LIST_SEP
      "repeat_sequence_header=5000000" OPTION_VALUE_LIST_SEP
      "h264_i_frame_period=30" OPTION_VALUE_LIST_SEP
      "h264_level=11" OPTION_VALUE_LIST_SEP
      "h264_profile=4" OPTION_VALUE_LIST_SEP
      "h264_minimum_qp_value=16" OPTION_VALUE_LIST_SEP
      "h264_maximum_qp_value=32"
  }
};

http_server_options_t http_options = {
  .port = 8080,
  .maxcons = 10
};

log_options_t log_options = {
  .debug = false,
  .verbose = false
};

rtsp_options_t rtsp_options = {
  .port = 0,
};

option_value_t camera_formats[] = {
  { "DEFAULT", 0 },
  { "YUYV", V4L2_PIX_FMT_YUYV },
  { "YUV420", V4L2_PIX_FMT_YUV420 },
  { "YUYV", V4L2_PIX_FMT_YUYV },
  { "MJPG", V4L2_PIX_FMT_MJPEG },
  { "MJPEG", V4L2_PIX_FMT_MJPEG },
  { "JPEG", V4L2_PIX_FMT_MJPEG },
  { "H264", V4L2_PIX_FMT_H264 },
  { "RG10", V4L2_PIX_FMT_SRGGB10 },
  { "GB10P", V4L2_PIX_FMT_SGRBG10P },
  { "RG10P", V4L2_PIX_FMT_SRGGB10P },
  { "RGB565", V4L2_PIX_FMT_RGB565 },
  { "RGBP", V4L2_PIX_FMT_RGB565 },
  { "RGB24", V4L2_PIX_FMT_RGB24 },
  { "RGB", V4L2_PIX_FMT_RGB24 },
  { "BGR", V4L2_PIX_FMT_BGR24 },
  {}
};

option_value_t camera_type[] = {
  { "v4l2", CAMERA_V4L2 },
  { "libcamera", CAMERA_LIBCAMERA },
  {}
};

option_t all_options[] = {
  DEFINE_OPTION_PTR(camera, path, string, "Chooses the camera to use. If empty connect to default."),
  DEFINE_OPTION_VALUES(camera, type, camera_type, "Select camera type."),
  DEFINE_OPTION(camera, width, uint, "Set the camera capture width."),
  DEFINE_OPTION(camera, height, uint, "Set the camera capture height."),
  DEFINE_OPTION_VALUES(camera, format, camera_formats, "Set the camera capture format."),
  DEFINE_OPTION(camera, nbufs, uint, "Set number of capture buffers. Preferred 2 or 3."),
  DEFINE_OPTION(camera, fps, uint, "Set the desired capture framerate."),
  DEFINE_OPTION_DEFAULT(camera, allow_dma, bool, "1", "Prefer to use DMA access to reduce memory copy."),
  DEFINE_OPTION(camera, high_res_factor, float, "Set the desired high resolution output scale factor."),
  DEFINE_OPTION(camera, low_res_factor, float, "Set the desired low resolution output scale factor."),
  DEFINE_OPTION_PTR(camera, options, list, "Set the camera options. List all available options with `-camera-list_options`."),
  DEFINE_OPTION(camera, auto_reconnect, uint, "Set the camera auto-reconnect delay in seconds."),
  DEFINE_OPTION_DEFAULT(camera, auto_focus, bool, "1", "Do auto-focus on start-up (does not work with all camera)."),
  DEFINE_OPTION_DEFAULT(camera, vflip, bool, "1", "Do vertical image flip (does not work with all camera)."),
  DEFINE_OPTION_DEFAULT(camera, hflip, bool, "1", "Do horizontal image flip (does not work with all camera)."),

  DEFINE_OPTION_PTR(camera, isp.options, list, "Set the ISP processing options. List all available options with `-camera-list_options`."),

  DEFINE_OPTION_PTR(camera, snapshot.options, list, "Set the JPEG compression options. List all available options with `-camera-list_options`."),
  DEFINE_OPTION(camera, snapshot.height, uint, "Override the snapshot height and maintain aspect ratio."),

  DEFINE_OPTION_DEFAULT(camera, stream.disabled, bool, "1", "Disable stream."),
  DEFINE_OPTION_PTR(camera, stream.options, list, "Set the JPEG compression options. List all available options with `-camera-list_options`."),
  DEFINE_OPTION(camera, stream.height, uint, "Override the stream height and maintain aspect ratio."),

  DEFINE_OPTION_DEFAULT(camera, video.disabled, bool, "1", "Disable video."),
  DEFINE_OPTION_PTR(camera, video.options, list, "Set the H264 encoding options. List all available options with `-camera-list_options`."),
  DEFINE_OPTION(camera, video.height, uint, "Override the video height and maintain aspect ratio."),

  DEFINE_OPTION_DEFAULT(camera, list_options, bool, "1", "List all available options and exit."),

  DEFINE_OPTION(http, port, uint, "Set the HTTP web-server port."),
  DEFINE_OPTION(http, maxcons, uint, "Set maximum number of concurrent HTTP connections."),

  DEFINE_OPTION_DEFAULT(rtsp, port, uint, "8554", "Set the RTSP server port (default: 8854)."),

  DEFINE_OPTION_DEFAULT(log, debug, bool, "1", "Enable debug logging."),
  DEFINE_OPTION_DEFAULT(log, verbose, bool, "1", "Enable verbose logging."),
  DEFINE_OPTION_PTR(log, filter, list, "Enable debug logging from the given files. Ex.: `-log-filter=buffer.cc`"),

  {}
};
