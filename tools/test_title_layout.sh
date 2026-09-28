#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m12-layout-tests"}
host_cc=${HOST_CC:-cc}

mkdir -p "$build_directory"
"$host_cc" -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/include" \
    "$project_root/source/title_layout.c" \
    "$project_root/tests/test_title_layout.c" \
    -o "$build_directory/test_title_layout"

"$build_directory/test_title_layout"
