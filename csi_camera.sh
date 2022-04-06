#!/bin/bash

set -xeo pipefail
make
./camera_stream -camera-path=$(echo /dev/v4l/by-path/*.csi-video-index0) "$@"
