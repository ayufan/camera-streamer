#!/bin/bash

DIR=$(realpath "$(dirname "$0")/..")
rm -rf "$DIR/tmp/deps"
mkdir -p "$DIR/tmp/deps"

set -xeo pipefail

cd third_party/mpp
git clean -fdx
cmake . -DBUILD_SHARED_LIBS=OFF -DBUILD_TEST=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$DIR/tmp/deps"
make -j5 install
cd ../..

# Cleanup unneeded files to avoid linking to .so files
rm -rf tmp/deps/lib/librockchip*.so*
