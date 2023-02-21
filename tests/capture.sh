#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd "$SCRIPT_DIR"

set -xeo pipefail

[[ -e capture.jpeg ]] || libcamera-jpeg --timeout 1000 --output capture.jpeg --width 1920 --height 1080 --encoding jpg
[[ -e capture.yuv420 ]] || libcamera-jpeg --timeout 1000 --output capture.yuv420 --width 1920 --height 1080 --encoding yuv420
[[ -e capture.h264 ]] || libcamera-vid --frames 1 --output capture.h264 --width 1920 --height 1080 --codec h264 --profile main --level 4.2

# This is not ideal as `libcamera-raw` does not respect `--frames`
[[ -e capture.bg10p ]] || libcamera-raw --frames 1 --timeout 300 --verbose 2 --flush --output capture.bg10p --width 2304 --height 1296
