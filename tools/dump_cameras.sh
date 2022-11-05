#!/bin/bash

set -x

v4l2-ctl --list-devices

for device in /dev/video*; do
  v4l2-ctl -d "$device" -L
  v4l2-ctl -d "$device" --list-formats-out
  v4l2-ctl -d "$device" --list-formats-ext
done
