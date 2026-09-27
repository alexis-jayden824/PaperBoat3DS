#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m13-tests"}
host_cc=${HOST_CC:-cc}
test_binary="$build_directory/test_m13"

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

if grep -n '#include[[:space:]]*<3ds.h>' "$project_root/source/f3d.c" \
    "$project_root/source/tex.c" "$project_root/source/runtime.c" \
    "$project_root/include/pb3ds/f3d.h" "$project_root/source/main.c"; then
    fail "Fast3D/runtime must not include 3ds.h"
fi
if grep -nE 'PBWorldScene|fake Mario|toad.?town' "$project_root/source/"*.c; then
    fail "M13 must not ship fake gameplay scenes"
fi
if grep -nE 'SDL_|glfw|imgui|Game\.cpp' "$project_root/source/f3d.c" \
    "$project_root/source/runtime.c"; then
    fail "M13 must not use desktop engines"
fi
grep -q 'PB_F3D_G_VTX' "$project_root/include/pb3ds/f3d.h" || fail "missing Fast3D opcodes"
test -s "$project_root/shaders/default.v.pica" || fail "missing PICA passthrough shader"
grep -q 'pb_tev_set_combine' "$project_root/source/tev.c" || fail "missing TEV combiner"
grep -q 'step_game_loop' "$project_root/source/runtime.c" || fail "runtime must name step_game_loop"
grep -q 'gfx_draw_frame' "$project_root/source/runtime.c" || fail "runtime must name gfx_draw_frame"
grep -q 'list_paperboat_sources' "$project_root/tools/list_paperboat_sources.sh" ||
    fail "missing game source lister"
grep -q 'm13-runtime-test' "$project_root/Makefile" || fail "Makefile missing m13-runtime-test"
grep -q 'Test M13 runtime contract' "$project_root/.github/workflows/3ds-build.yml" ||
    fail "CI must run the M13 host contract"
grep -q 'PaperBoat3DS-Refolded.cia' "$project_root/.github/workflows/3ds-build.yml" ||
    fail "CI must still upload CIA"
if grep -n 'boot_main(' "$project_root/source/main.c"; then
    fail "main must not call boot_main (infinite nuGfx wait)"
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
    "$project_root/source/title.c" \
    "$project_root/source/tex.c" \
    "$project_root/source/tev.c" \
    "$project_root/source/f3d.c" \
    "$project_root/source/runtime.c" \
    "$project_root/tests/test_m13.c" \
    -lm \
    -o "$test_binary"

"$test_binary"
echo "M13 runtime contract passed"
