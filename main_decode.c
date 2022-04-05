#include "buffer.h"
#include "buffer_list.h"
#include "device.h"
#include "links.h"
#include "v4l2.h"
#include "http.h"

#include <signal.h>

int camera_width = 1280;
int camera_height = 720;
int camera_format = V4L2_PIX_FMT_SRGGB10P;
int camera_nbufs = 1;
bool camera_use_low = true;

device_t *camera = NULL;
device_t *isp_yuuv = NULL;
device_t *codec_jpeg = NULL;
device_t *codec_h264 = NULL;

int open_isp(buffer_list_t *src, const char *srgb_path, const char *yuuv_path, const char *yuuv_low_path)
{
  if (device_open_buffer_list(isp_yuuv, false, src->fmt_width, src->fmt_height, src->fmt_format, camera_nbufs) < 0 ||
    device_open_buffer_list(isp_yuuv, true, src->fmt_width, src->fmt_height, V4L2_PIX_FMT_YUYV, camera_nbufs) < 0) {
    return -1;
  }

  return 0;
}

int open_jpeg(buffer_list_t *src, const char *tmp)
{
  DEVICE_SET_OPTION2(codec_jpeg, JPEG, COMPRESSION_QUALITY, 80);

  if (device_open_buffer_list(codec_jpeg, false, src->fmt_width, src->fmt_height, src->fmt_format, camera_nbufs) < 0 ||
    device_open_buffer_list(codec_jpeg, true, src->fmt_width, src->fmt_height, V4L2_PIX_FMT_JPEG, camera_nbufs) < 0) {
    return -1;
  }
  return 0;
}

int open_h264(buffer_list_t *src, const char *tmp)
{
  DEVICE_SET_OPTION2(codec_h264, MPEG_VIDEO, BITRATE, 5000 * 1000);
  DEVICE_SET_OPTION2(codec_h264, MPEG_VIDEO, H264_I_PERIOD, 30);
  DEVICE_SET_OPTION2(codec_h264, MPEG_VIDEO, H264_LEVEL, V4L2_MPEG_VIDEO_H264_LEVEL_4_0);
  DEVICE_SET_OPTION2(codec_h264, MPEG_VIDEO, REPEAT_SEQ_HEADER, 1);
  DEVICE_SET_OPTION2(codec_h264, MPEG_VIDEO, H264_MIN_QP, 16);
  DEVICE_SET_OPTION2(codec_h264, MPEG_VIDEO, H264_MIN_QP, 32);

  if (device_open_buffer_list(codec_h264, false, src->fmt_width, src->fmt_height, src->fmt_format, camera_nbufs) < 0 ||
    device_open_buffer_list(codec_h264, true, src->fmt_width, src->fmt_height, V4L2_PIX_FMT_H264, camera_nbufs) < 0) {
    return -1;
  }
  return 0;
}

int open_camera()
{
  camera = device_open("CAMERA", "/dev/video0");

  if (device_open_buffer_list(camera, true, camera_width, camera_height, camera_format, camera_nbufs) < 0) {
    return -1;
  }

  DEVICE_SET_OPTION(camera, EXPOSURE, 1148);
  DEVICE_SET_OPTION(camera, ANALOGUE_GAIN, 938);
  DEVICE_SET_OPTION(camera, DIGITAL_GAIN, 256);

  isp_yuuv = device_open("ISP-YUUV", "/dev/video12");
  codec_jpeg = device_open("JPEG", "/dev/video31");
  codec_h264 = device_open("H264", "/dev/video11");
  //codec_h264->allow_dma = false;

  if (open_isp(camera->capture_list, "/dev/video13", "/dev/video14", "/dev/video15") < 0) {
    return -1;
  }
  if (open_jpeg(camera_use_low ? isp_yuuv->capture_list : isp_yuuv->capture_list, "/dev/video31") < 0) {
    return -1;
  }
  if (open_h264(camera_use_low ? isp_yuuv->capture_list : isp_yuuv->capture_list, "/dev/video11") < 0) {
    return -1;
  }

  return 0;
}

bool check_streaming()
{
  return http_jpeg_needs_buffer() || http_h264_needs_buffer();
}

int main(int argc, char *argv[])
{
  if (open_camera() < 0) {
    return -1;
  }

  link_t links[] = {
    {
      camera,
      { isp_yuuv },
      { NULL, check_streaming }
    },
    {
      isp_yuuv,
      {
        codec_jpeg,
      },
      { NULL, NULL }
    },
    {
      codec_jpeg,
      { },
      { http_jpeg_capture, http_jpeg_needs_buffer }
    },
    {
      // codec_h264,
      // { },
      // { http_h264_capture, http_h264_needs_buffer }
    },
    { NULL }
  };

  http_method_t http_methods[] = {
    { "GET / ", http_index },
    { "GET /snapshot ", http_snapshot },
    { "GET /stream ", http_stream },
    { "GET /?action=snapshot ", http_snapshot },
    { "GET /?action=stream ", http_stream },
    { "GET /video ", http_video_html },
    { "GET /video.h264 ", http_video },
    { "GET /jmuxer.min.js ", http_jmuxer_js },
    { NULL, NULL }
  };

  sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);

  int http_fd = http_server(9092, 5, http_methods);

  bool running = false;
  links_loop(links, &running);

  close(http_fd);

error:
  device_close(isp_yuuv);
  device_close(camera);
  return 0;
}
