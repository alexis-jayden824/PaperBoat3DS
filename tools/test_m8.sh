#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m8-tests"}
host_cc=${HOST_CC:-cc}
test_binary="$build_directory/test_m8"

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

if grep -n '#include[[:space:]]*<3ds.h>' "$project_root/source/input.c" \
    "$project_root/source/compat.c" "$project_root/include/pb3ds/input.h"; then
    fail "input mapping must not include 3ds.h"
fi
if grep -nE 'SDL2|libultraship' "$project_root/source/input.c"; then
    fail "M8 must not use SDL or desktop libultraship"
fi
grep -q 'PB_KEY_SELECT' "$project_root/include/pb3ds/input.h" ||
    fail "SELECT must remain a named HID bit"
grep -Fq 'pb_input_select_reserved' "$project_root/source/input.c" ||
    fail "SELECT must be reserved in the mapper"
grep -q 'm8-input-test' "$project_root/Makefile" || fail "Makefile missing m8-input-test"
grep -q 'Test M8 input contract' "$project_root/.github/workflows/3ds-build.yml" ||
    fail "CI must run the M8 host contract"
grep -q 'PaperBoat3DS-Refolded.cia' "$project_root/.github/workflows/3ds-build.yml" ||
    fail "CI must still upload CIA"

mkdir -p "$build_directory"
"$host_cc" \
    -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/include" \
    "$project_root/source/bootstrap.c" \
    "$project_root/source/platform.c" \
    "$project_root/source/diag.c" \
    "$project_root/source/input.c" \
    "$project_root/source/fs.c" \
    "$project_root/source/compat.c" \
    "$project_root/tests/test_m8.c" \
    -o "$test_binary"

"$test_binary"
echo "M8 input contract passed"
