#!/bin/bash
set -e

BUILD_TYPE=${1:-debug}
BUILD_DIR="build-ohos"

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Configure and build
cd "$BUILD_DIR"
cmake -DCMAKE_TOOLCHAIN_FILE=$OHOS_NATIVE_HOME/build/cmake/ohos.toolchain.cmake \
      -DOHOS_ARCH=x86_64 \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      ..

make -j$(nproc)
