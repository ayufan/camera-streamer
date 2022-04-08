#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd "$SCRIPT_DIR"

set -xeo pipefail
make -j$(nproc)
$GDB ./camera_stream -camera-path=$(echo /dev/v4l/by-path/*.csi-video-index0) \
  -camera-options=vertical_blanking=728 \
  -camera-options=exposure=2444 \
  -camera-options=analogue_gain=600 \
  -camera-options=digital_gain=256 \
  -camera-isp.options=digital_gain=2015 \
  -camera-isp.options=red_balance=1852 \
  -camera-isp.options=blue_balance=2146 \
  -camera-isp.options=colour_correction_matrix=1,0,0,0,146,4,0,0,232,3,0,0,211,255,255,255,232,3,0,0,132,255,255,255,232,3,0,0,91,255,255,255,232,3,0,0,34,5,0,0,232,3,0,0,107,255,255,255,232,3,0,0,37,0,0,0,232,3,0,0,226,254,255,255,232,3,0,0,225,4,0,0,232,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0 \
  -camera-isp.options=black_level=1,0,0,0,0,16,0,16,0,16,38,134 \
  -camera-isp.options=green_equalisation=1,0,0,0,89,2,0,0,47,0,0,0,232,3,0,0 \
  -camera-isp.options=gamma=1,0,0,0,0,0,0,4,0,8,0,12,0,16,0,20,0,24,0,28,0,32,0,36,0,40,0,44,0,48,0,52,0,56,0,60,0,64,0,72,0,80,0,88,0,96,0,104,0,112,0,120,0,128,0,144,0,160,0,176,0,192,0,208,0,224,0,240,255,255,0,0,176,19,122,36,68,48,208,59,131,70,54,81,153,90,144,100,38,109,83,117,5,125,252,132,115,140,155,147,236,153,213,159,194,170,34,180,42,188,132,195,237,201,85,207,197,211,36,216,86,224,159,230,207,235,185,240,80,245,160,249,109,253,255,255 \
  -camera-isp.options=denoise=1,0,0,0,0,0,0,0,240,30,0,0,232,3,0,0,238,2,0,0,232,3,0,0 \
  -camera-isp.options=sharpen=1,0,0,0,208,7,0,0,232,3,0,0,244,1,0,0,232,3,0,0,244,1,0,0,232,3,0,0 \
  -camera-isp.options=defective_pixel_correction=1,0,0,0,1,0,0,0 \
  -camera-isp.options=colour_denoise=0,0,0,0,127,0,0,0 \
  "$@"
