#!/bin/bash

COLOR_NC='\033[0m' # No Color
COLOR_WHITE='\033[1;37m'
COLOR_BLACK='\033[0;30m'
COLOR_BLUE='\033[0;34m'
COLOR_LIGHT_BLUE='\033[1;34m'
COLOR_GREEN='\033[0;32m'
COLOR_LIGHT_GREEN='\033[1;32m'
COLOR_CYAN='\033[0;36m'
COLOR_LIGHT_CYAN='\033[1;36m'
COLOR_RED='\033[0;31m'
COLOR_LIGHT_RED='\033[1;31m'
COLOR_PURPLE='\033[0;35m'
COLOR_LIGHT_PURPLE='\033[1;35m'
COLOR_BROWN='\033[0;33m'
COLOR_YELLOW='\033[1;33m'
COLOR_GRAY='\033[1;30m'
COLOR_LIGHT_GRAY='\033[0;37m'

COLORS=( $COLOR_BLUE $COLOR_CYAN $COLOR_PURPLE $COLOR_BROWN $COLOR_YELLOW $COLOR_GRAY )

run_camera() {
  local name="$1"
  shift

  job_nb=$(jobs -p -r | wc -l)
  color_idx=$(($job_nb%${#COLORS[@]}))
  color=${COLORS[$color_idx]}
  port=$((8080+$job_nb))

  ./camera-streamer --http-port=$port --camera-fps=20 --camera-force_active --log-stats "$@" |&
    ts "$(printf "$color%%H:%%M:%%S %10s |$COLOR_NC" "$name")" &
}

trap 'jobs -p -r | xargs -r kill; wait' EXIT

export FAKE_CAMERA_SENSOR=arducam_64mp=imx519

set -eo pipefail

make -j$(nproc)

run_camera csi --camera-type=libcamera --camera-format=YUYV --camera-path=i2c0mux --camera-fps=20 "$@"

for usbcam in /dev/v4l/by-id/usb-*-video-index0; do
  name=$(basename "$usbcam")
  name="${name#usb-}"
  name="${name%%-video-index0}"
  name="${name:0:10}"
  run_camera "$name" --camera-path="$usbcam" --camera-format=MJPEG --camera-fps=30 "$@"
done

wait
