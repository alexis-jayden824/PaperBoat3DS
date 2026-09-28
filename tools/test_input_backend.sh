#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m8-tests"}
host_cc=${HOST_CC:-cc}
test_binary="$build_directory/test_input"

mkdir -p "$build_directory"
"$host_cc" \
    -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/tests/mocks" \
    -I"$project_root/include" \
    "$project_root/source/input.c" \
    "$project_root/tests/test_input.c" \
    -o "$test_binary"

"$test_binary"

"$host_cc" \
    -std=c11 -Wall -Wextra -Werror -fsyntax-only \
    -I"$project_root/tests/mocks" \
    -I"$project_root/include" \
    "$project_root/source/main.c"
