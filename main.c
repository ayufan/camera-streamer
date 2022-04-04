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

  if (device_open_buffer_list(camera, true, camera_width, camera_height, camera_format, camera_nbufs, true) < 0) {
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

  if (device_open_buffer_list(isp_srgb, false, src->fmt_width, src->fmt_height, src->fmt_format, camera_nbufs, true) < 0 ||
    device_open_buffer_list(isp_yuuv, true, src->fmt_width, src->fmt_height, V4L2_PIX_FMT_YUYV, camera_nbufs, true) < 0 ||
    device_open_buffer_list(isp_yuuv_low, true, src->fmt_width / 2, src->fmt_height / 2, V4L2_PIX_FMT_YUYV, camera_nbufs, true) < 0) {
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

  if (device_open_buffer_list(codec_jpeg, false, src->fmt_width, src->fmt_height, src->fmt_format, camera_nbufs, false) < 0 ||
    device_open_buffer_list(codec_jpeg, true, src->fmt_width, src->fmt_height, V4L2_PIX_FMT_JPEG, camera_nbufs, false) < 0) {
    return -1;
  }

  return 0;
}

void connect_links(device_t **links) {
  buffer_t *buf, *output_buf;
  device_t *src = links[0];

  if (src->capture_list->do_mmap) {
    if (buffer_list_wait_pool(src->capture_list, 100, true)) {
      if (buf = buffer_list_dequeue(src->capture_list, true)) {
        for (int i = 1; links[i]; i++) {
          buffer_list_enqueue(links[i]->output_list, buf);

          if (buffer_list_wait_pool(links[i]->output_list, 10, false)) {
            // consume dma-bufs
            if (output_buf = buffer_list_dequeue(links[i]->output_list, false)) {
              buffer_consumed(output_buf);
            }
          }
        }
        buffer_consumed(buf);
      }

    }
  } else {
    for (int i = 1; links[i]; i++) {
      if (buffer_list_wait_pool(links[i]->output_list, 100, true)) {
        if (output_buf = buffer_list_dequeue(links[i]->output_list, true)) {
          buffer_list_enqueue(src->capture_list, output_buf);
          buffer_consumed(output_buf);
        }
      }
    }

    // consume dma-bufs
    if (buffer_list_wait_pool(src->capture_list, 100, false)) {
      if (buf = buffer_list_dequeue(src->capture_list, false)) {
        buffer_consumed(output_buf);
      }
    }
  }
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

  device_t *links[][10] = {
    {
      camera,
      isp_srgb,
      NULL
    }, {
      isp_yuuv,
      codec_jpeg,
      NULL
    }, {
      isp_yuuv_low,
      NULL
    }, {
      codec_jpeg,
      NULL
    },
    NULL
  };

  while(true) {
    for (int i = 0; links[i] && links[i][0]; i++) {
      connect_links(links[i]);
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
