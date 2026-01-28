#!/bin/bash
set -e

BUILD_TYPE=${1:-debug}
BUILD_DIR="build"

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Configure and build
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
make -j$(nproc)

# Run tests
echo "------------------------------------------------"
echo "Running tests via CTest..."
echo "------------------------------------------------"
ctest --output-on-failure
