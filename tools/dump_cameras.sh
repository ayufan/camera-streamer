#!/bin/bash

exec 2>&1

( set -x ; echo "$(cat /sys/firmware/devicetree/base/model | tr -d '\0')" )

( set -x ; uname -a )

( set -x ; v4l2-ctl --list-devices )

for device in /dev/video*; do
  echo "===================================="
  echo "DEVICE: $device"
  echo "===================================="
  (
    set -x
    v4l2-ctl -d "$device" --info \
    --list-formats-ext --list-fields \
    --list-formats-out --list-fields-out
  ) | while IFS= read -r LINE; do
    printf "%s | %s\n" "$device" "$LINE"
  done
  echo
done
