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

source $CANGJIE_HOME/envsetup.sh

function cjc-android() {
  cjc --target aarch64-linux-android26 \
    --sysroot "$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot/" \
    -L "$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/lib/clang/17/lib/linux" \
    $@
}

function clang-android() {
  "$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang" $@
}

clang-android \
  "${SCRIPT_DIR}/${LAUNCHER_NAME}.c" \
  -Os \
  '-Wl,-z,noexecstack' \
  '-pie' \
  '-rdynamic' \
  -fno-omit-frame-pointer \
  -o "${BUILD_DIR}/${LAUNCHER_NAME}" \
  "-L$CANGJIE_HOME/runtime/lib/linux_android_aarch64_cjnative" \
  '-l:libcangjie-runtime.so' \
  '-ldl' '-lc' \

cp *.cj ${BUILD_DIR}

clang-android -D_GNU_SOURCE -c trampoline.c -o ${BUILD_DIR}/trampoline.o

cd ${BUILD_DIR}

cjc-android trampoline.cj --output-type=staticlib
ar rcs libtrampoline.a trampoline.o

cjc-android libtrampoline.a entry.cj --output-type=dylib
cjc-android cbcengine-helper.cj --output-type=dylib -o libcbcengine-helper.so
