#!/usr/bin/env bash

set -euo pipefail

readonly SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
readonly ENGINE_DIR=$(realpath "${SCRIPT_DIR}/../../..")
readonly GENMC="${ENGINE_DIR}/../genmc/build17-gcc/bin/genmc"

if [[ ! -x "${GENMC}" ]]; then
    echo "GenMC executable not found: ${GENMC}" >&2
    exit 2
fi

"${GENMC}" \
    --disable-estimation \
    -- \
    -std=c++17 \
    -DCBC_PROFILER_CALL_BUFFER_SIZE=128 \
    -I"${SCRIPT_DIR}/include" \
    -I"${ENGINE_DIR}/src" \
    "${SCRIPT_DIR}/fiber_profile_test.cc"
