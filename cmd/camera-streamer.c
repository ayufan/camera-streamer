#include "http/http.h"
#include "opts/opts.h"
#include "opts/log.h"
#include "opts/fourcc.h"
#include "device/camera/camera.h"
#include "rtsp/rtsp.h"

#include <signal.h>
#include <unistd.h>

extern unsigned char html_index_html[];
extern unsigned int html_index_html_len;
extern unsigned char html_video_html[];
extern unsigned int html_video_html_len;
extern unsigned char html_jmuxer_min_js[];
extern unsigned int html_jmuxer_min_js_len;

camera_t *camera;

void *camera_http_set_option(http_worker_t *worker, FILE *stream, const char *key, const char *value, void *headersp)
{
  bool *headers = headersp;

  if (!camera) {
    if (!*headers) {
      http_500(stream, "");
      *headers = true;
    }
    fprintf(stream, "No camera attached.\r\n");
    return NULL;
  }

  bool set = false;

  for (int i = 0; i < MAX_DEVICES; i++) {
    if (device_set_option_string(camera->devices[i], key, value) == 0) {
      set = true;
      break;
    }
  }

  if (set) {
    if (!*headers) {
      http_200(stream, "");
      *headers = true;
    }
    fprintf(stream, "The '%s' was set to '%s'.\r\n", key, value);
  } else {
    if (!*headers) {
      http_500(stream, "");
      *headers = true;
    }
    fprintf(stream, "Cannot set '%s' to '%s'.\r\n", key, value);
  }

  return NULL;
}

void camera_http_option(http_worker_t *worker, FILE *stream)
{
  bool headers = false;
  http_enum_params(worker, stream, camera_http_set_option, &headers);
  if (!headers) {
    http_404(stream, "");
    fprintf(stream, "No options passed.\r\n");
  }

  fprintf(stream, "\r\nSet: /option?name=value\r\n\r\n");

  if (camera) {
    for (int i = 0; i < MAX_DEVICES; i++) {
      device_dump_options(camera->devices[i], stream);
    }
  }
}

http_method_t http_methods[] = {
  { "GET /snapshot?", http_snapshot },
  { "GET /snapshot.jpg?", http_snapshot },
  { "GET /stream?", http_stream },
  { "GET /?action=snapshot", http_snapshot },
  { "GET /?action=stream", http_stream },
  { "GET /video?", http_content, "text/html", html_video_html, 0, &html_video_html_len },
  { "GET /video.h264?", http_h264_video },
  { "GET /video.mkv?", http_mkv_video },
  { "GET /video.mp4?", http_mp4_video },
  { "GET /option?", camera_http_option },
  { "GET /jmuxer.min.js?", http_content, "text/javascript", html_jmuxer_min_js, 0, &html_jmuxer_min_js_len },
  { "GET /?", http_content, "text/html", html_index_html, 0, &html_index_html_len },
  { }
};

camera_options_t camera_options = {
  .path = "",
  .width = 1920,
  .height = 1080,
  .format = 0,
  .nbufs = 3,
  .fps = 30,
  .allow_dma = true,
  .high_res_factor = 1.0,
  .low_res_factor = 0.0,
  .auto_reconnect = 0,
  .auto_focus = true,
  .options = "",
  .list_options = false,
  .h264 = {
    .options =
      "video_bitrate_mode=0" OPTION_VALUE_LIST_SEP
      "video_bitrate=10000000" OPTION_VALUE_LIST_SEP
      "repeat_sequence_header=5000000" OPTION_VALUE_LIST_SEP
      "h264_i_frame_period=30" OPTION_VALUE_LIST_SEP
      "h264_level=11" OPTION_VALUE_LIST_SEP
      "h264_profile=4" OPTION_VALUE_LIST_SEP
      "h264_minimum_qp_value=16" OPTION_VALUE_LIST_SEP
      "h264_maximum_qp_value=32"
  },
  .jpeg = {
    .options = "compression_quality=80"
  },
};

http_server_options_t http_options = {
  .port = 8080,
  .maxcons = 10
};

log_options_t log_options = {
  .debug = false,
  .verbose = false
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
  DEFINE_OPTION_PTR(camera, jpeg.options, list, "Set the JPEG compression options. List all available options with `-camera-list_options`."),
  DEFINE_OPTION_PTR(camera, h264.options, list, "Set the H264 encoding options. List all available options with `-camera-list_options`."),
  DEFINE_OPTION_DEFAULT(camera, list_options, bool, "1", "List all available options and exit."),

  DEFINE_OPTION(http, port, uint, "Set the HTTP web-server port."),
  DEFINE_OPTION(http, maxcons, uint, "Set maximum number of concurrent HTTP connections."),

  DEFINE_OPTION_DEFAULT(log, debug, bool, "1", "Enable debug logging."),
  DEFINE_OPTION_DEFAULT(log, verbose, bool, "1", "Enable verbose logging."),
  DEFINE_OPTION_PTR(log, filter, list, "Enable debug logging from the given files. Ex.: `-log-filter=buffer.cc`"),

  {}
};

int main(int argc, char *argv[])
{
  int http_fd = -1;
  int ret = -1;

  if (parse_opts(all_options, argc, argv) < 0) {
    return -1;
  }

  if (camera_options.list_options) {
    camera = camera_open(&camera_options);
    if (camera) {
      printf("\n");
      for (int i = 0; i < MAX_DEVICES; i++) {
        device_dump_options(camera->devices[i], stdout);
      }
      camera_close(&camera);
    }
    return -1;
  }

  http_fd = http_server(&http_options, http_methods);
  if (http_fd < 0) {
    goto error;
  }

  rtsp_server();

  while (true) {
    camera = camera_open(&camera_options);
    if (camera) {
      ret = camera_run(camera);
      camera_close(&camera);
    }

    if (camera_options.auto_reconnect > 0) {
      LOG_INFO(NULL, "Automatically reconnecting in %d seconds...", camera_options.auto_reconnect);
      sleep(camera_options.auto_reconnect);
    } else {
      break;
    }
  }

error:
  close(http_fd);
  return ret;
}
