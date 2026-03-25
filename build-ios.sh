#!/bin/bash
set -e

BUILD_TYPE=${1:-Debug}
BUILD_DIR="build"

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Configure and build
cmake -S . -B "$BUILD_DIR" -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=NO

cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel $(sysctl -n hw.ncpu)
