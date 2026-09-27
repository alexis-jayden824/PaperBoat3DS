#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m6-tests"}
host_cc=${HOST_CC:-cc}
test_binary="$build_directory/test_m6"

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

if grep -n '#include[[:space:]]*<3ds.h>' "$project_root/source/compat.c"; then
    fail "compat.c must not include 3ds.h"
fi
if grep -nE 'libultraship/|SDL2/|#include <imgui' "$project_root/source/compat.c" \
    "$project_root/include/pb3ds/compat.h"; then
    fail "M6 must not include desktop libultraship, SDL, or ImGui"
fi
grep -q -- '-f cia' "$project_root/Makefile" || fail "Makefile must build CIA"
grep -q 'PaperBoat3DS-Refolded.cia' "$project_root/.github/workflows/3ds-build.yml" ||
    fail "CI must upload the CIA"
test -s "$project_root/packaging/banner.png" || fail "missing packaging/banner.png"
test -s "$project_root/packaging/banner.wav" || fail "missing packaging/banner.wav"

mkdir -p "$build_directory"
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
    "$project_root/tests/test_m6.c" \
    -o "$test_binary"

"$test_binary"
echo "M6 compatibility contract passed"
