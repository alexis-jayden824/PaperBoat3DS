#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m9-tests"}
host_cc=${HOST_CC:-cc}
test_binary="$build_directory/test_renderer"

mkdir -p "$build_directory"
"$host_cc" \
    -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/include" \
    "$project_root/source/renderer.c" \
    "$project_root/tests/test_renderer.c" \
    -o "$test_binary"

"$test_binary"
