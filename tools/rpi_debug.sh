#!/bin/bash

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <on|off>"
  exit 1
fi

debug="0"
[[ "$1" != "on" ]] || debug=0xFFFFFF

set -x

for module in /sys/module/bcm2835_*; do
  echo $debug | tee $module/parameters/debug
done
