#!/bin/bash

# Requires:
# $1 - cangjie toolchain location
# $2 - android ndk location

SCRIPT_DIR=$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"; pwd)
BUILD_DIR="${SCRIPT_DIR}/build"
mkdir -p "$BUILD_DIR"

CANGJIE_HOME=$1
ANDROID_NDK=$2
LAUNCHER_NAME=launcher
PRELOAD_NAME=libcbc-preload.so

source $CANGJIE_HOME/envsetup.sh

function cjc-android() {
  cjc --fobf-layout --fno-obf-export-symbols --fobf-source-path --obf-sym-output-mapping libcbcengine-helper.obf.map \
    --target aarch64-linux-android26 \
    --sysroot "$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot/" \
    -L "$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/lib/clang/17/lib/linux" \
    $@
}

function clang-android() {
  "$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang" $@
}

clang-android \
  "${SCRIPT_DIR}/engine-common.c" \
  -Os \
  -fno-omit-frame-pointer \
  -c -o "${BUILD_DIR}/engine-common.o"

clang-android \
  "${SCRIPT_DIR}/${LAUNCHER_NAME}.c" \
  -Os \
  '-Wl,-z,noexecstack' \
  '-pie' \
  '-rdynamic' \
  -fno-omit-frame-pointer \
  -o "${BUILD_DIR}/${LAUNCHER_NAME}" \
  "${BUILD_DIR}/engine-common.o" \
  "-L$CANGJIE_HOME/runtime/lib/linux_android_aarch64_cjnative" \
  '-l:libcangjie-runtime.so' \
  '-ldl' '-lc' \

clang-android \
  "${SCRIPT_DIR}/preload.c" \
  -Os \
  '-Wl,-z,noexecstack' \
  -fno-omit-frame-pointer \
  -shared -o "${BUILD_DIR}/${PRELOAD_NAME}" \
  "${BUILD_DIR}/engine-common.o" \
  '-ldl' '-lc' \

cp *.cj ${BUILD_DIR}

cd ${BUILD_DIR}

cjc-android cbcengine-helper.cj --output-type=dylib -o libcbcengine-helper.so
