#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 /path/to/model-checker" >&2
    exit 2
fi

checker_dir=$(realpath "$1")
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
model_binary="$checker_dir/test/cbc_profiler_protocol.o"

make -C "$checker_dir" libmodel.so \
    CPPFLAGS='-Wall -g -O3 -std=gnu++11 -Iinclude -I.'

g++ -std=gnu++11 -Wall -Wextra -g -O2 \
    -ffile-prefix-map="$script_dir/"= \
    -I"$checker_dir" \
    -isystem "$checker_dir/include" \
    -o "$model_binary" \
    "$script_dir/profiler_protocol.cc" \
    -L"$checker_dir" \
    -lmodel

"$checker_dir/run.sh" "$model_binary" -m 2 -y
