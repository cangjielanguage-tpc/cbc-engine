#!/bin/bash

set -xeuo pipefail

SCRIPT_DIR=$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"; pwd)
BUILD_DIR="${SCRIPT_DIR}/build"
mkdir -p "$BUILD_DIR"

CANGJIE_HOME=$1
LAUNCHER_NAME=launcher
PRELOAD_NAME=libcbc-preload.so

for src in launcher preload engine-common; do
  clang -c -Os -fPIC "${SCRIPT_DIR}/${src}.c" -fno-omit-frame-pointer -o "${BUILD_DIR}/${src}.o"
done

'clang' \
  '-o' "${BUILD_DIR}/${LAUNCHER_NAME}" \
  '-Wl,-z,noexecstack' \
  '-pie' \
  '-rdynamic' \
  "-L$CANGJIE_HOME/lib/linux_x86_64_cjnative" \
  "-L$CANGJIE_HOME/runtime/lib/linux_x86_64_cjnative" \
  '-T' "$CANGJIE_HOME/lib/linux_x86_64_cjnative/cjld.lds" \
  "${BUILD_DIR}/launcher.o" "${BUILD_DIR}/engine-common.o" \
  '-l:libcangjie-std-core.so' \
  '-l:libcangjie-runtime.so' \
  '-lsecurec' '-ldl' '-lm' '-lc' \
  '--rtlib=compiler-rt'

'clang' \
  '-shared' '-o' "${BUILD_DIR}/${PRELOAD_NAME}" \
  '-Wl,-z,noexecstack' \
  "${BUILD_DIR}/preload.o" "${BUILD_DIR}/engine-common.o" \
  '-ldl'
cp *.cj ${BUILD_DIR}

cd ${BUILD_DIR}

cjc --fobf-layout --fno-obf-export-symbols --fobf-source-path --obf-sym-output-mapping libcbcengine-helper.obf.map \
cbcengine-helper.cj --output-type=dylib -o libcbcengine-helper.so
