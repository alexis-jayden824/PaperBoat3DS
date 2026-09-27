#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m3-tests"}
host_cc=${HOST_CC:-cc}
test_binary="$build_directory/test_platform"

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

# PaperBoat-facing headers and the application entry must not pull in 3ds.h.
if grep -R --include='*.h' -n '#include[[:space:]]*[<"]3ds\.h[>"]' \
    "$project_root/include/pb3ds"; then
    fail "include/pb3ds must not include 3ds.h"
fi
if grep -n '#include[[:space:]]*<3ds.h>' "$project_root/source/main.c" \
    "$project_root/source/bootstrap.c" "$project_root/source/diag.c" \
    "$project_root/source/assets.c" "$project_root/source/input.c" \
    "$project_root/source/fs.c"; then
    fail "main.c, bootstrap.c, diag.c, assets.c, input.c, and fs.c must not include 3ds.h"
fi
grep -q '#include[[:space:]]*<3ds.h>' "$project_root/source/platform.c" ||
    fail "platform.c is the only permitted 3ds.h owner"

mkdir -p "$build_directory"
"$host_cc" \
    -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/include" \
    "$project_root/source/bootstrap.c" \
    "$project_root/source/platform.c" \
    "$project_root/source/diag.c" \
    "$project_root/source/assets.c" \
    "$project_root/source/input.c" \
    "$project_root/source/fs.c" \
    "$project_root/tests/test_platform.c" \
    -o "$test_binary"

"$test_binary"
echo "M3 platform contract passed"
