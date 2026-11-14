#!/usr/bin/env bash
# Run under devkitPro MSYS2, with the pinned dependencies prepared.
set -euo pipefail
cd "$(dirname "$0")/../.."
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export DEVKITA64="$DEVKITPRO/devkitA64"
export PATH="$DEVKITA64/bin:$DEVKITPRO/tools/bin:/ucrt64/bin:/usr/bin:$PATH"
jobs=${JOBS:-6}
output="$PWD/build-nx/NX-build"
native="$output/renderer"
cmake -S tools/nx/dxvk -B "$native" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DNX_DXVK_SOURCE="$PWD/build-nx/deps/dxvk-sarek-1.14.1"
cmake --build "$native" -j "$jobs"
for mode in sp mp; do
  sp=OFF
  if [[ "$mode" == sp ]]; then sp=ON; fi
  build="$output/$mode"
  cmake -S . -B "$build" -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
    -DCMAKE_BUILD_TYPE=Release -DNX_SP="$sp" -DNX_DXVK=ON \
    -DKISAK_DIRECT_CONNECT=ON -DOPENBLOPS_NX_OUTPUT_DIR="$output" \
    -DNX_MESA_SDK="$PWD/build-nx/deps/mesa-26.2.7/opt/devkitpro/portlibs/switch" \
    -U Vulkan_DIR -U Vulkan_LIBRARY -U Vulkan_INCLUDE_DIR \
    -DNX_DXVK_PREBUILT_DIR="$native"
  cmake --build "$build" -j "$jobs"
done
