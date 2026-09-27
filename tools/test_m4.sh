#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m4-tests"}
host_cc=${HOST_CC:-cc}
test_binary="$build_directory/test_diag"

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

# New 3DS extras must be detected, never enabled in M4.
grep -q 'APT_CheckNew3DS' "$project_root/source/platform.c" ||
    fail "platform.c must detect New 3DS"
if grep -n 'osSetSpeedupEnable\|APT_SetAppCpuTimeLimit' \
    "$project_root/source/"*.c "$project_root/include/pb3ds/"*.h; then
    fail "M4 must not enable New 3DS speedup or extra APPCORE time"
fi
if grep -n '#include[[:space:]]*<3ds.h>' "$project_root/source/diag.c" \
    "$project_root/source/main.c" "$project_root/source/assets.c"; then
    fail "diag.c, main.c, and assets.c must not include 3ds.h"
fi

mkdir -p "$build_directory"
"$host_cc" \
    -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/include" \
    "$project_root/source/bootstrap.c" \
    "$project_root/source/platform.c" \
    "$project_root/source/diag.c" \
    "$project_root/tests/test_diag.c" \
    -o "$test_binary"

"$test_binary"
echo "M4 diagnostics contract passed"
