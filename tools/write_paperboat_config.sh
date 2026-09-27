#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
lock="$project_root/upstream/PAPERBOAT.lock"
out=${1:-"$project_root/build/paperboat_config.h"}
root=${PB3DS_PAPERBOAT_ROOT:-"$project_root/.cache/upstream/PaperBoat"}

value() {
    awk -F= -v key="$1" '$1 == key { print $2; found=1 } END { exit found ? 0 : 1 }' "$lock"
}

commit=$(value PAPERBOAT_COMMIT)
release=$(value PAPERBOAT_RELEASE)
mkdir -p "$(dirname "$out")"

if [ -f "$root/src/port/decode_yay0.c" ] && [ -f "$root/src/port/libc_compat.c" ]; then
    cat >"$out" <<EOF
#pragma once
#define PB3DS_HAS_PAPERBOAT_SLICE 1
#define PB3DS_PAPERBOAT_SLICE_LIBC "$root/src/port/libc_compat.c"
#define PB3DS_PAPERBOAT_SLICE_YAY0 "$root/src/port/decode_yay0.c"
EOF
else
    cat >"$out" <<EOF
#pragma once
#define PB3DS_HAS_PAPERBOAT_SLICE 0
EOF
fi

echo "wrote $out (commit=$commit release=$release slice=$( [ -f "$root/src/port/decode_yay0.c" ] && echo 1 || echo 0 ))"
