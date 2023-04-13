#include "camera.h"

#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "device/device_list.h"
#include "device/links.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"
#include "device/buffer_list.h"
#include "util/http/http.h"
#include "output/output.h"

// Taken from: https://github.com/roamingthings/spyglass
static unsigned char jpeg_exif_orientation[] =
{
  // 0xFF, 0xD8, // Start of Image (SOI) marker
  0xFF, 0xE1, // APP1 marker
  0x00, 0x62, // Length of APP 1 segment (98 bytes)
  0x45, 0x78, 0x69, 0x66, // EXIF identifier ("Exif" in ASCII)
  0x00, 0x00, // Padding bytes
  // TIFF header (with big-endian indicator)
  0x4D, 0x4D, // Big endian
  0x00, 0x2A, // TIFF magic number
  0x00, 0x00, 0x00, 0x08, // Offset to first IFD (8 bytes)
  // Image File Directory (IFD)
  0x00, 0x05, // Number of entries in the IFD (5)
  // v-- Orientation tag (tag number = 0x0112, type = USHORT, count = 1)
  0x01, 0x12, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
  0x00, 0xFF, 0x00, 0x00, // Tag data
  // v-- XResolution tag (tag number = 0x011A, type = UNSIGNED RATIONAL, count = 1)
  0x01, 0x1A, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x4A, // Tag data (address)
  // v-- YResolution tag (tag number = 0x011B, type = UNSIGNED RATIONAL, count = 1)
  0x01, 0x1B, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x52, // Tag data (address)
  // v-- ResolutionUnit tag (tag number = 0x0128, type = USHORT, count = 1)
  0x01, 0x28, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x02, 0x00, 0x00, // 2 - Inch
  // v-- YCbCrPositioning tag (tag number = 0x0213, type = USHORT, count = 1)
  0x02, 0x13, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x01, 0x00, 0x00, // center of pixel array
  0x00, 0x00, 0x00, 0x00, // Offset to next IFD 0
  0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x01, // XResolution value
  0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x01  // # YResolution value
};

static bool buffer_insert_at(buffer_t *buf, size_t at, const void *data, size_t length)
{
  // check if there's enough space
  if (buf->used + length > buf->length)
    return false;
  if (buf->used < at)
    return false;

  char *ptr = buf->start;
  memmove(&ptr[at + length], &ptr[at], buf->used - at);
  memcpy(&ptr[at], data, length);
  buf->used += length;
  return true;
}

void jpeg_exif_orientation_set(int orientation)
{
  jpeg_exif_orientation[29] = orientation;
}

void jpeg_exif_orientation_on_buffer(buffer_t *buf)
{
  char *ptr = buf->start;

  // check start and end bytes
  if (buf->used <= 4)
    return;
  if (ptr[0] != 0xFF || ptr[1] != 0xD8)
    return;
  if (ptr[buf->used-2] != 0xFF || ptr[buf->used-1] != 0xD9)
    return;

#if 1
  buffer_insert_at(buf, 2, jpeg_exif_orientation, sizeof(jpeg_exif_orientation));
#else
  buffer_insert_at(buf, buf->used-2, jpeg_exif_orientation, sizeof(jpeg_exif_orientation));
#endif
}

// Taken from https://stackoverflow.com/q/62089397
static unsigned char h264_display_sei[] =
{
  0, 0, 0, 1, // prefix
  6, // header
  47, 3, // Display orientation type, payload size

  // var displayOrientationCancelFlag = "0"; // u(1); Rotation information follows
  // var horFlip = "1"; // hor_flip; u(1); Flip horizontally
  // var verFlip = "1"; // ver_flip; u(1); Flip vertically
  // var anticlockwiseRotation = "0100000000000000"; // u(16); varue / 2^16 -> 90 degrees
  // var displayOrientationRepetitionPeriod = "010"; // ue(v); Persistent till next video sequence
  // var displayOrientationExtensionFlag = "0"; // u(1); No other varue is permitted by the spec atm
  // var byteAlignment = "1";
  104, 0, 9,
  0x80 // end of SEI
};

void h264_display_sei_on_buffer(buffer_t *buf)
{
  if (buf->used <= 5)
    return;
  if (!buf->flags.is_keyframe)
    return;
#if 0
  buffer_insert_at(buf, 0, h264_display_sei, sizeof(h264_display_sei));
#else
  buffer_insert_at(buf, buf->used, h264_display_sei, sizeof(h264_display_sei));
#endif
}

void h264_display_sei_set(int orientation)
{
  // jpeg_exif_orientation[29] = orientation;
}
