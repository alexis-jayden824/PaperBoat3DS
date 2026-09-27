#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
lock="$project_root/upstream/PAPERBOAT.lock"
dest=${PB3DS_TORCH_ROOT:-"$project_root/.cache/upstream/Torch-LH"}

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

value() {
    awk -F= -v key="$1" '$1 == key { print $2; found=1 } END { exit found ? 0 : 1 }' "$lock"
}

url=$(value TORCH_REPOSITORY)
commit=$(value TORCH_COMMIT)
[ -n "$url" ] && [ -n "$commit" ] || fail "TORCH lock is incomplete"

mkdir -p "$(dirname "$dest")"

if [ -d "$dest/.git" ]; then
    git -C "$dest" fetch --depth 1 origin "$commit"
else
    git clone --filter=blob:none --sparse --no-checkout "$url" "$dest"
    git -C "$dest" sparse-checkout set README.md CMakeLists.txt LICENSE
    git -C "$dest" fetch --depth 1 origin "$commit"
fi

git -C "$dest" checkout --detach "$commit"
head=$(git -C "$dest" rev-parse HEAD)
[ "$head" = "$commit" ] || fail "Torch HEAD $head != pin $commit"
[ -f "$dest/README.md" ] || fail "missing Torch README.md"
[ -f "$dest/CMakeLists.txt" ] || fail "missing Torch CMakeLists.txt"
grep -Fq './torch otr' "$dest/README.md" || fail "Torch README missing otr invocation"

printf 'fetched Torch-LH %s into %s\n' "$commit" "$dest"
