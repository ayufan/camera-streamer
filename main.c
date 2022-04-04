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

int open_isp(const char *srgb_path, const char *yuuv_path, const char *yuuv_low_path)
{
  isp_srgb = device_open("ISP-SRGB", srgb_path);
  isp_yuuv = device_open("ISP-YUUV", yuuv_path);
  isp_yuuv_low = device_open("ISP-YUUV-LOW", yuuv_low_path);

  if (!isp_srgb || !isp_yuuv || !isp_yuuv_low) {
    return -1;
  }

  if (device_open_buffer_list(isp_srgb, false, camera_width, camera_height, camera_format, camera_nbufs) < 0 ||
    device_open_buffer_list(isp_yuuv, true, camera_width, camera_height, V4L2_PIX_FMT_YUYV, camera_nbufs) < 0 ||
    device_open_buffer_list(isp_yuuv, true, camera_width / 2, camera_height / 2, V4L2_PIX_FMT_YUYV, camera_nbufs) < 0) {
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[])
{
  if (open_camera("/dev/video0") < 0) {
    goto error;
  }

  // if (open_isp("/dev/video13", "/dev/video14", "/dev/video15") < 0) {
  //   goto error;
  // }

// return;

  if (device_stream(camera, true) < 0) {
    goto error;
  }
  //return;

  while(true) {
    if (buffer_list_wait_pool(camera->capture_list, 1000000)) {
      buffer_t *buf;
      if (buf = buffer_list_capture_dequeue(camera->capture_list)) {
        E_LOG_INFO(camera, "Got camera buffer: %p", buf);
        buffer_capture_enqueue(buf);
      }
    }
  }

error:
  device_close(isp_yuuv_low);
  device_close(isp_yuuv);
  device_close(isp_srgb);
  device_close(camera);
  return 0;
}
