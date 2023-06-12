#include "sw.h"

#include "device/device.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"

#ifdef USE_LIBJPEG
#include <jpeglib.h>

bool sw_device_process_jpeg_capture_buf(buffer_t *output_buf, buffer_t *capture_buf)
{
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  int ret;

  LOG_VERBOSE(output_buf, "JPEG parsing: %ld", output_buf->used);

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, output_buf->start, output_buf->used);

  ret = jpeg_read_header(&cinfo, TRUE);
  if (ret != JPEG_HEADER_OK) {
    LOG_VERBOSE(output_buf, "JPEG invalid header: %d", ret);
    goto error;
  }

  if (capture_buf->buf_list->fmt.width != cinfo.image_width &&
    capture_buf->buf_list->fmt.height != cinfo.image_height) {
    LOG_VERBOSE(output_buf, "JPEG invalid image size: %dx%d, expected %dx%d",
      cinfo.image_width, cinfo.image_height,
      capture_buf->buf_list->fmt.width, capture_buf->buf_list->fmt.height);
    goto error;
  }

  cinfo.scale_num = 1;
  cinfo.scale_denom = 1;
  cinfo.dct_method = JDCT_FASTEST; // JDCT_DEFAULT
  cinfo.out_color_space = JCS_YCbCr;

  if (!jpeg_start_decompress(&cinfo)) {
    LOG_VERBOSE(output_buf, "Failed to start decompress");
    goto error;
  }

  capture_buf->length = 0;

  while (cinfo.output_scanline < cinfo.output_height) {
    JSAMPROW row = (unsigned char *)capture_buf->start + capture_buf->length;
    ret = jpeg_read_scanlines(&cinfo, &row, 1);
    if (!ret) {
      LOG_VERBOSE(output_buf, "Failed to read scanline");
      goto error;
    }
    capture_buf->length += ret * cinfo.output_width * 2;
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  return true;

error:
  jpeg_destroy_decompress(&cinfo);
  return false;
}
#else // USE_LIBJPEG
bool sw_device_process_jpeg_capture_buf(buffer_t *output_buf, buffer_t *capture_buf)
{
  return false;
}
#endif // USE_LIBJPEG
