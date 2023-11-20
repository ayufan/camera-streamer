#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd "$SCRIPT_DIR/.."

CAMERA_PATH=( $(echo /dev/v4l/by-id/usb-*-video-index0) )

set -xeo pipefail
make -j$(nproc)
$GDB ./camera-streamer \
  -camera-path="${CAMERA_PATH[${CAMERA_INDEX:-0}]}" \
  --http-listen=0.0.0.0 \
  "$@"
