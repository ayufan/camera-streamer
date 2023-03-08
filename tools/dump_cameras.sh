#!/bin/bash

exec 2>&1

prefix() {
  while IFS= read -r LINE; do
    printf "%s | %s\n" "$1" "$LINE"
  done
}

( set -x ; echo "$(cat /sys/firmware/devicetree/base/model | tr -d '\0')" )

( set -x ; uname -a )

( set -x ; v4l2-ctl --list-devices ) | prefix "v4l2-ctl"

( set -x ; libcamera-hello --list-cameras ) | prefix "libcamera"

for device in /dev/video*; do
  echo "===================================="
  echo "DEVICE: $device"
  echo "===================================="
  (
    set -x
    v4l2-ctl -d "$device" --info \
    --list-formats-ext --list-fields \
    --list-formats-out --list-fields-out
  ) | prefix "$device"
  echo
done
