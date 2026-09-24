#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m0-tests"}
host_cc=${HOST_CC:-cc}
test_binary="$build_directory/test_bootstrap"

mkdir -p "$build_directory"
"$host_cc" \
    -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/include" \
    "$project_root/source/bootstrap.c" \
    "$project_root/tests/test_bootstrap.c" \
    -o "$test_binary"

"$test_binary"
