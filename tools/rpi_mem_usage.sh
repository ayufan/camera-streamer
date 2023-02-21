#!/bin/bash

get_mem() {
  local mem=$(vcgencmd get_mem "$1" | cut -d= -f2)
  shift
  echo -e "$@\t\t$mem"
}

(
  get_mem arm "Total ARM"
  get_mem gpu "Total GPU"
  get_mem malloc_total "GPU Malloc"
  get_mem malloc "GPU Malloc Free"
  get_mem reloc_total "GPU Reloc"
  get_mem reloc "GPU Reloc Free"
  cat /sys/kernel/debug/vcsm-cma/state | grep "SIZE" | awk '{s+=$2} END {printf("CMA\t\t\t%.1fMB\n", s/1024/1024)}'
)

echo "--------"
vcdbg reloc | sed -s 's/^/reloc: /'

echo "--------"
vcdbg malloc | sed -s 's/^/maloc: /'
echo "--------"
cat /sys/kernel/debug/vcsm-cma/state | sed -s 's/^/cma: /'
echo "--------"
