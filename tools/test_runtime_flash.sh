#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/runtime-flash-tests"}
host_cc=${HOST_CC:-cc}
test_binary="$build_directory/test_runtime_flash"
test_file="$build_directory/test-runtime.sav"

mkdir -p "$build_directory"
"$host_cc" -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/include" \
    "$project_root/source/runtime_flash.c" \
    "$project_root/tests/test_runtime_flash.c" \
    -o "$test_binary"
"$test_binary" "$test_file"
