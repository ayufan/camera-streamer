#!/bin/bash

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <input-file> [camera-streamer options]"
  echo
  echo "examples:"
  echo "  $0 tests/capture.jpeg"
  echo "  $0 tests/capture.jpeg --video-height=720"
  echo "  $0 tests/capture.jpeg --snapshot-height=720 --video-height=480"
  echo "  $0 tests/capture.h264"
  exit 1
fi

INPUT=$(realpath "$1")
shift

case "$INPUT" in
  *.jpeg)
    set -- --camera-format=JPEG --camera-width=1920 --camera-height=1080 "$@"
    ;;

  *.yuv420)
    set -- --camera-format=YUV420 --camera-width=1920 --camera-height=1080 "$@"
    ;;

  *.h264)
    set -- --camera-format=H264 --camera-width=1920 --camera-height=1080 "$@"
    ;;

  *.bg10p)
    set -- --camera-format=BG10P --camera-width=2304 --camera-height=1296 "$@"
    ;;

  *)
    echo "$0: undefined format for $INPUT."
    exit 1
esac

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd "$SCRIPT_DIR/.."

set -xeo pipefail
make -j$(nproc)

exec ./camera-streamer \
  --camera-type=dummy \
  --camera-path="$INPUT" \
  --camera-snapshot.height=1080 \
  "$@"
