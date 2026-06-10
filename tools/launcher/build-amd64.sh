#!/bin/bash

set -xeuo pipefail

SCRIPT_DIR=$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"; pwd)
BUILD_DIR="${SCRIPT_DIR}/build"
mkdir -p "$BUILD_DIR"

CANGJIE_HOME=$1
LAUNCHER_NAME=launcher

clang -c -Os "${SCRIPT_DIR}/${LAUNCHER_NAME}.c" -fno-omit-frame-pointer -o "${BUILD_DIR}/${LAUNCHER_NAME}.o"

'clang' \
  '-o' "${BUILD_DIR}/${LAUNCHER_NAME}" \
  '-Wl,-z,noexecstack' \
  '-pie' \
  '-rdynamic' \
  "-L$CANGJIE_HOME/lib/linux_x86_64_cjnative" \
  "-L$CANGJIE_HOME/runtime/lib/linux_x86_64_cjnative" \
  '-T' "$CANGJIE_HOME/lib/linux_x86_64_cjnative/cjld.lds" \
  "${BUILD_DIR}/${LAUNCHER_NAME}.o" \
  '-l:libcangjie-std-core.so' \
  '-l:libcangjie-runtime.so' \
  '-lsecurec' '-ldl' '-lm' '-lc' \
  '--rtlib=compiler-rt'

cp *.cj ${BUILD_DIR}

clang -D_GNU_SOURCE -c trampoline.c -o ${BUILD_DIR}/trampoline.o

cd ${BUILD_DIR}

cjc trampoline.cj --output-type=staticlib
ar rcs libtrampoline.a trampoline.o

cjc libtrampoline.a entry.cj --output-type=dylib
