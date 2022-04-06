#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd "$SCRIPT_DIR"

set -xeo pipefail
make
$GDB ./camera_stream -camera-path=$(echo /dev/v4l/by-path/*.csi-video-index0) "$@"
