#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
lock="$project_root/upstream/PAPERBOAT.lock"
dest=${PB3DS_PAPERBOAT_ROOT:-"$project_root/.cache/upstream/PaperBoat"}

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

value() {
    awk -F= -v key="$1" '$1 == key { print $2; found=1 } END { exit found ? 0 : 1 }' "$lock"
}

url=$(value PAPERBOAT_REPOSITORY)
commit=$(value PAPERBOAT_COMMIT)
[ -n "$url" ] && [ -n "$commit" ] || fail "PAPERBOAT lock is incomplete"

mkdir -p "$(dirname "$dest")"

if [ -d "$dest/.git" ]; then
    git -C "$dest" fetch --depth 1 origin "$commit"
else
    git clone --filter=blob:none --sparse --no-checkout "$url" "$dest"
    git -C "$dest" sparse-checkout set \
        CMakeLists.txt \
        src/port/libc_compat.c \
        src/port/decode_yay0.c
    git -C "$dest" fetch --depth 1 origin "$commit"
fi

git -C "$dest" checkout --detach "$commit"
head=$(git -C "$dest" rev-parse HEAD)
[ "$head" = "$commit" ] || fail "PaperBoat HEAD $head != pin $commit"

[ -f "$dest/CMakeLists.txt" ] || fail "missing CMakeLists.txt"
[ -f "$dest/src/port/libc_compat.c" ] || fail "missing libc_compat.c"
[ -f "$dest/src/port/decode_yay0.c" ] || fail "missing decode_yay0.c"

printf 'fetched PaperBoat %s into %s\n' "$commit" "$dest"
