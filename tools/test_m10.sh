#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m10-tests"}
host_cc=${HOST_CC:-cc}
test_binary="$build_directory/test_m10"

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

if grep -n '#include[[:space:]]*<3ds.h>' "$project_root/source/loop.c" \
    "$project_root/include/pb3ds/time.h"; then
    fail "loop.c must not include 3ds.h"
fi
if grep -nE 'SDL_|glfw|PollEvent' "$project_root/source/loop.c" "$project_root/source/main.c"; then
    fail "M10 must not use a desktop window loop"
fi
if grep -n 'threadCreate\|svcCreateThread' "$project_root/source/loop.c" \
    "$project_root/source/main.c"; then
    fail "M10 must not start extra OS threads"
fi
grep -q 'PB_LOOP_TICK_HZ' "$project_root/include/pb3ds/time.h" || fail "missing 30Hz tick"
grep -q 'm10-loop-test' "$project_root/Makefile" || fail "Makefile missing m10-loop-test"
grep -q 'Test M10 loop contract' "$project_root/.github/workflows/3ds-build.yml" ||
    fail "CI must run the M10 host contract"
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
    "$project_root/source/loop.c" \
    "$project_root/source/compat.c" \
    "$project_root/source/gfx.c" \
    "$project_root/tests/test_m10.c" \
    -lm \
    -o "$test_binary"

"$test_binary"
echo "M10 loop contract passed"
