#include "buffer.h"
#include "buffer_list.h"
#include "device.h"
#include "v4l2.h"

int camera_width = 1920;
int camera_height = 1080;
int camera_format = V4L2_PIX_FMT_SRGGB10P;
int camera_nbufs = 4;

device_t *camera = NULL;
device_t *isp_srgb = NULL;
device_t *isp_yuuv = NULL;
device_t *isp_yuuv_low = NULL;
device_t *codec_jpeg = NULL;

int open_camera(const char *path)
{
  camera = device_open("CAMERA", path);
  if (!camera) {
    return -1;
  }

  if (device_open_buffer_list(camera, true, camera_width, camera_height, camera_format, camera_nbufs) < 0) {
    return -1;
  }

  return 0;
}

int open_isp(buffer_list_t *src, const char *srgb_path, const char *yuuv_path, const char *yuuv_low_path)
{
  isp_srgb = device_open("ISP-SRGB", srgb_path);
  isp_yuuv = device_open("ISP-YUUV", yuuv_path);
  isp_yuuv_low = device_open("ISP-YUUV-LOW", yuuv_low_path);

  if (!isp_srgb || !isp_yuuv || !isp_yuuv_low) {
    return -1;
  }

  if (device_open_buffer_list(isp_srgb, false, src->fmt_width, src->fmt_height, src->fmt_format, camera_nbufs) < 0 ||
    device_open_buffer_list(isp_yuuv, true, src->fmt_width, src->fmt_height, V4L2_PIX_FMT_YUYV, camera_nbufs) < 0 ||
    device_open_buffer_list(isp_yuuv_low, true, src->fmt_width / 2, src->fmt_height / 2, V4L2_PIX_FMT_YUYV, camera_nbufs) < 0) {
    return -1;
  }

  return 0;
}

int open_jpeg(buffer_list_t *src, const char *jpeg_codec)
{
  codec_jpeg = device_open("JPEG", jpeg_codec);
  if (!codec_jpeg) {
    return -1;
  }

  if (device_open_buffer_list(codec_jpeg, false, src->fmt_width, src->fmt_height, src->fmt_format, camera_nbufs) < 0 ||
    device_open_buffer_list(codec_jpeg, true, src->fmt_width, src->fmt_height, V4L2_PIX_FMT_JPEG, camera_nbufs) < 0) {
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[])
{
  if (open_camera("/dev/video0") < 0) {
    goto error;
  }
  if (open_isp(camera->capture_list, "/dev/video13", "/dev/video14", "/dev/video15") < 0) {
    goto error;
  }
  if (open_jpeg(isp_yuuv->capture_list, "/dev/video31") < 0) {
    goto error;
  }

// return;

  if (device_stream(camera, true) < 0) {
    goto error;
  }
  if (device_stream(isp_srgb, true) < 0) {
    goto error;
  }
  if (device_stream(isp_yuuv, true) < 0) {
    goto error;
  }
  if (device_stream(isp_yuuv_low, true) < 0) {
    goto error;
  }
  if (device_stream(codec_jpeg, true) < 0) {
    goto error;
  }

  while(true) {
    buffer_t *buf;

    if (buffer_list_wait_pool(camera->capture_list, 100)) {
      if (buf = buffer_list_mmap_dequeue(camera->capture_list)) {
        buffer_list_mmap_enqueue(isp_srgb->output_list, buf);
        buffer_consumed(buf);
      }
    }

    if (buffer_list_wait_pool(isp_yuuv->capture_list, 100)) {
      if (buf = buffer_list_mmap_dequeue(isp_yuuv->capture_list)) {
        buffer_list_mmap_enqueue(codec_jpeg->output_list, buf);
        buffer_consumed(buf);
      }
    }

    if (buffer_list_wait_pool(isp_yuuv_low->capture_list, 100)) {
      if (buf = buffer_list_mmap_dequeue(isp_yuuv_low->capture_list)) {
        buffer_consumed(buf);
      }
    }

    if (buffer_list_wait_pool(codec_jpeg->capture_list, 100)) {
      if (buf = buffer_list_mmap_dequeue(codec_jpeg->capture_list)) {
        buffer_consumed(buf);
      }
    }

    usleep(100*1000);
  }

error:
  device_close(isp_yuuv_low);
  device_close(isp_yuuv);
  device_close(isp_srgb);
  device_close(camera);
  return 0;
}
