#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m11-tests"}
host_cc=${HOST_CC:-cc}
test_binary="$build_directory/test_m11"

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

if grep -n '#include[[:space:]]*<3ds.h>' "$project_root/source/gfx.c" \
    "$project_root/include/pb3ds/gfx.h" "$project_root/source/compat.c" \
    "$project_root/source/main.c"; then
    fail "portable gfx/compat/main must not include 3ds.h"
fi
if grep -n '#include[[:space:]]*<citro3d.h>' "$project_root/source/gfx.c" \
    "$project_root/source/platform.c" "$project_root/source/main.c"; then
    fail "citro3d.h belongs only in gfx_pica.c"
fi
grep -q '#include[[:space:]]*<citro3d.h>' "$project_root/source/gfx_pica.c" ||
    fail "gfx_pica.c must include citro3d.h"
grep -q '#include[[:space:]]*<3ds.h>' "$project_root/source/gfx_pica.c" ||
    fail "gfx_pica.c must include 3ds.h"
grep -q '#include[[:space:]]*<3ds.h>' "$project_root/source/platform.c" ||
    fail "platform.c remains a 3ds.h owner"
if grep -nE 'toad.?town|Toad Town|goomba|peach.?castle|mac_00' \
    "$project_root/source/gfx.c" "$project_root/source/gfx_pica.c" \
    "$project_root/source/main.c"; then
    fail "M11 must not ship fake gameplay geometry"
fi
if grep -nE 'SDL_|glfw|GL/gl|vulkan|imgui' "$project_root/source/gfx.c" \
    "$project_root/source/gfx_pica.c"; then
    fail "M11 must not use desktop GPU APIs"
fi
if ls "$project_root"/source/*.v.pica "$project_root"/source/*.g.pica \
    "$project_root"/source/*.shbin 2>/dev/null; then
    fail "M11 does not vendor picasso shaders"
fi
grep -q -- '-lcitro3d' "$project_root/Makefile" || fail "Makefile must link citro3d"
grep -q 'm11-gfx-test' "$project_root/Makefile" || fail "Makefile missing m11-gfx-test"
grep -q 'Test M11 gfx contract' "$project_root/.github/workflows/3ds-build.yml" ||
    fail "CI must run the M11 host contract"
grep -q 'PaperBoat3DS-Refolded.cia' "$project_root/.github/workflows/3ds-build.yml" ||
    fail "CI must still upload CIA"
if grep -n 'boot_main' "$project_root/source/main.c" "$project_root/source/gfx.c"; then
    fail "M11 must not call boot_main"
fi

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
    "$project_root/tests/test_m11.c" \
    -lm \
    -o "$test_binary"

"$test_binary"
echo "M11 gfx contract passed"
