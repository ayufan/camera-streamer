#include "buffer.h"
#include "buffer_list.h"
#include "device.h"
#include "links.h"
#include "v4l2.h"

int camera_width = 1920;
int camera_height = 1080;
int camera_format = V4L2_PIX_FMT_SRGGB10P;
int camera_nbufs = 4;
bool camera_use_low = false;

device_t *camera = NULL;
device_t *isp_srgb = NULL;
device_t *isp_yuuv = NULL;
device_t *isp_yuuv_low = NULL;
device_t *codec_jpeg = NULL;

int open_isp(buffer_list_t *src, const char *srgb_path, const char *yuuv_path, const char *yuuv_low_path)
{
  if (device_open_buffer_list(isp_srgb, false, src->fmt_width, src->fmt_height, src->fmt_format, camera_nbufs) < 0 ||
    device_open_buffer_list(isp_yuuv, true, src->fmt_width, src->fmt_height, V4L2_PIX_FMT_YUYV, camera_nbufs) < 0 ||
    device_open_buffer_list(isp_yuuv_low, true, src->fmt_width / 2, src->fmt_height / 2, V4L2_PIX_FMT_YUYV, camera_nbufs) < 0) {
    return -1;
  }

  return 0;
}

int open_jpeg(buffer_list_t *src, const char *tmp)
{
  if (device_open_buffer_list(codec_jpeg, false, src->fmt_width, src->fmt_height, src->fmt_format, camera_nbufs) < 0 ||
    device_open_buffer_list(codec_jpeg, true, src->fmt_width, src->fmt_height, V4L2_PIX_FMT_JPEG, camera_nbufs) < 0) {
    return -1;
  }

  return 0;
}

void write_jpeg(buffer_t *buf)
{
  FILE *fp = fopen("/tmp/capture.jpg.tmp", "wb");
  if (fp) {
    fwrite(buf->start, 1, buf->used, fp);
    fclose(fp);
    E_LOG_DEBUG(buf, "Wrote JPEG: %d", buf->used);
  }
  rename("/tmp/capture.jpg.tmp", "/tmp/capture.jpg");
}

int main(int argc, char *argv[])
{
  camera = device_open("CAMERA", "/dev/video0");

  if (device_open_buffer_list(camera, true, camera_width, camera_height, camera_format, camera_nbufs) < 0) {
    return -1;
  }

  isp_srgb = device_open("ISP-SRGB", "/dev/video13");
  //isp_srgb->allow_dma = false;
  isp_yuuv = device_open("ISP-YUUV", "/dev/video14");
  isp_yuuv->upstream_device = isp_srgb;
  isp_yuuv_low = device_open("ISP-YUUV-LOW", "/dev/video15");
  isp_yuuv_low->upstream_device = isp_srgb;
  codec_jpeg = device_open("JPEG", "/dev/video31");
  codec_jpeg->allow_dma = false;

  if (open_isp(camera->capture_list, "/dev/video13", "/dev/video14", "/dev/video15") < 0) {
    goto error;
  }
  if (open_jpeg(camera_use_low ? isp_yuuv_low->capture_list : isp_yuuv->capture_list, "/dev/video31") < 0) {
    goto error;
  }

  link_t links[] = {
    {
      camera,
      { isp_srgb },
      NULL
    },
    {
      isp_yuuv,
      { camera_use_low ? NULL : codec_jpeg },
      NULL,
      V4L2_PIX_FMT_YUYV
    },
    {
      isp_yuuv_low,
      { camera_use_low ? codec_jpeg : NULL },
      NULL,
      V4L2_PIX_FMT_YUYV
    },
    {
      codec_jpeg,
      { },
      write_jpeg,
      V4L2_PIX_FMT_JPEG
    },
    { NULL }
  };

  // if (links_init(links) < 0) {
  //   return -1;
  // }

  bool running = false;
  links_loop(links, &running);

error:
  device_close(isp_yuuv_low);
  device_close(isp_yuuv);
  device_close(isp_srgb);
  device_close(camera);
  return 0;
}
