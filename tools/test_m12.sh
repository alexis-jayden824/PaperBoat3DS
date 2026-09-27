#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m12-tests"}
host_cc=${HOST_CC:-cc}
test_binary="$build_directory/test_m12"

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

if grep -n '#include[[:space:]]*<3ds.h>' "$project_root/source/title.c" \
    "$project_root/include/pb3ds/title.h" "$project_root/source/main.c"; then
    fail "title path must not include 3ds.h"
fi
if grep -nE 'state_title_screen|boot_main' "$project_root/source/title.c" \
    "$project_root/source/main.c"; then
    fail "M12 must not compile or call PaperBoat title/boot entry"
fi
if grep -nE 'toad.?town|Toad Town|goomba|peach.?castle' \
    "$project_root/source/title.c" "$project_root/source/main.c"; then
    fail "M12 must not ship fake gameplay geometry"
fi
if grep -nE 'SDL_|glfw|imgui' "$project_root/source/title.c"; then
    fail "M12 must not use desktop engines"
fi
grep -Fq '__OTR__title_screen/title_logo' "$project_root/include/pb3ds/title.h" ||
    fail "title.h must pin PaperBoat title_logo OTR name"
grep -Fq '89600' "$project_root/include/pb3ds/title.h" ||
    fail "title.h must pin US title_logo byte size"
grep -q 'pb_gfx_upright_t' "$project_root/source/gfx.c" ||
    fail "M12 requires upright T mapping"
grep -q 'm12-title-test' "$project_root/Makefile" || fail "Makefile missing m12-title-test"
grep -q 'Test M12 title contract' "$project_root/.github/workflows/3ds-build.yml" ||
    fail "CI must run the M12 host contract"
grep -q 'PaperBoat3DS-Refolded.cia' "$project_root/.github/workflows/3ds-build.yml" ||
    fail "CI must still upload CIA"
if git -C "$project_root" ls-files '*.o2r' | grep .; then
    fail "o2r archives must not be tracked"
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
    "$project_root/tests/test_m12.c" \
    -lm \
    -o "$test_binary"

"$test_binary"
echo "M12 title contract passed"
