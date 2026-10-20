#!/bin/bash
# script for backward compatibility with ci.
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"; pwd)
PROJECT_DIR="${SCRIPT_DIR}/../.."
BUILD_DIR="${SCRIPT_DIR}/build"
OUTPUT_DIR="${PROJECT_DIR}/output/linux_x86_64"

if [[ -n "${1:-}" ]]; then
  export CANGJIE_HOME="$1"
fi

python3 "${PROJECT_DIR}/build.py" build-launcher --target-os linux --target-arch x86_64 --cbcengine-lib "${OUTPUT_DIR}"
python3 "${PROJECT_DIR}/build.py" build-helper-lib --target-os linux --target-arch x86_64

mkdir -p "${BUILD_DIR}"
cp "${OUTPUT_DIR}/launcher/launcher" "${BUILD_DIR}/"
cp "${OUTPUT_DIR}/libcbcengine-helper.so" "${BUILD_DIR}/"
