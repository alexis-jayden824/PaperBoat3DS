#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m9-tests"}
host_cc=${HOST_CC:-cc}
test_binary="$build_directory/test_m9"
host_root="$build_directory/sdmc"

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

if grep -n '#include[[:space:]]*<3ds.h>' "$project_root/source/fs.c" \
    "$project_root/include/pb3ds/fs.h" "$project_root/source/compat.c"; then
    fail "fs.c must not include 3ds.h"
fi
if grep -nE 'StormLib|libultraship/|minizip' "$project_root/source/fs.c"; then
    fail "M9 must not pull desktop archive engines"
fi
grep -q 'PB_FS_ALIGN' "$project_root/include/pb3ds/fs.h" || fail "missing alignment"
grep -q 'm9-fs-test' "$project_root/Makefile" || fail "Makefile missing m9-fs-test"
grep -q 'Test M9 filesystem contract' "$project_root/.github/workflows/3ds-build.yml" ||
    fail "CI must run the M9 host contract"
grep -q 'PaperBoat3DS-Refolded.cia' "$project_root/.github/workflows/3ds-build.yml" ||
    fail "CI must still upload CIA"
if git -C "$project_root" ls-files '*.o2r' | grep .; then
    fail "o2r archives must not be tracked"
fi

mkdir -p "$host_root"
"$host_cc" \
    -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/include" \
    "$project_root/source/bootstrap.c" \
    "$project_root/source/platform.c" \
    "$project_root/source/diag.c" \
    "$project_root/source/input.c" \
    "$project_root/source/fs.c" \
    "$project_root/source/loop.c" \
    "$project_root/source/compat.c" \
    "$project_root/source/gfx.c" \
    "$project_root/tests/test_m9.c" \
    -lm \
    -o "$test_binary"

"$test_binary" "$host_root"
echo "M9 filesystem contract passed"
