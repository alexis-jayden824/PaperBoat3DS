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
if grep -n '#include[[:space:]]*[<"]3ds\.h[>"]' "$project_root/source/main.c" \
    "$project_root/source/bootstrap.c" "$project_root/source/diag.c" \
    "$project_root/source/assets.c" "$project_root/source/input.c" \
    "$project_root/source/fs.c" "$project_root/source/loop.c" \
    "$project_root/source/gfx.c" "$project_root/source/title.c" \
    "$project_root/source/compat.c"; then
    fail "PaperBoat-facing sources must not include 3ds.h"
fi
grep -q '#include[[:space:]]*<3ds.h>' "$project_root/source/platform.c" ||
    fail "platform.c must remain a 3ds.h owner"
grep -q '#include[[:space:]]*<3ds.h>' "$project_root/source/gfx_pica.c" ||
    fail "gfx_pica.c must be a 3ds.h GPU owner"

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
    "$project_root/source/loop.c" \
    "$project_root/source/gfx.c" \
    "$project_root/tests/test_platform.c" \
    -lm \
    -o "$test_binary"

"$test_binary"
echo "M3 platform contract passed"
