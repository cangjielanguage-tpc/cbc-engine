#!/bin/bash
set -e

# Build directory
BUILD_DIR="build"

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Configure and build
cd "$BUILD_DIR"
cmake ..
make -j$(nproc)

# Run tests
echo "------------------------------------------------"
echo "Running tests via CTest..."
echo "------------------------------------------------"
ctest --output-on-failure
