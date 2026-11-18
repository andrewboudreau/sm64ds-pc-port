#!/bin/sh
# Build and link the host audio test on Linux (g++ or clang++). Usage:
#   sh build_host_test.sh [outdir]      (default ./build)
set -e
here=$(cd "$(dirname "$0")" && pwd)
root="$here/../../hal/sdat"
out=${1:-"$here/build"}
mkdir -p "$out"
CXX=${CXX:-g++}
$CXX -std=c++14 -O2 -I"$root" \
$CXX -std=c++14 -O2 -Wall -Wno-unused-function -I"$here/shim" -I"$root" \
echo "built $out/host_audio_test"
