#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
lock="$project_root/upstream/PAPERBOAT.lock"
doc="$project_root/docs/M2.md"

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

[ -f "$lock" ] || fail "missing $lock"
[ -f "$doc" ] || fail "missing $doc"

value() {
    awk -F= -v key="$1" '$1 == key { print $2; found=1 } END { exit found ? 0 : 1 }' "$lock"
}

paperboat=$(value PAPERBOAT_COMMIT)
lus=$(value LIBULTRASHIP_COMMIT)
torch=$(value TORCH_COMMIT)
release=$(value PAPERBOAT_RELEASE)

printf '%s' "$paperboat" | grep -Eq '^[0-9a-f]{40}$' || fail "PAPERBOAT_COMMIT"
printf '%s' "$lus" | grep -Eq '^[0-9a-f]{40}$' || fail "LIBULTRASHIP_COMMIT"
printf '%s' "$torch" | grep -Eq '^[0-9a-f]{40}$' || fail "TORCH_COMMIT"
[ "$release" = "1.0.1" ] || fail "PAPERBOAT_RELEASE must be 1.0.1"

grep -Fq "$paperboat" "$doc" || fail "docs/M2.md missing PaperBoat commit"
grep -Fq "$lus" "$doc" || fail "docs/M2.md missing libultraship commit"
grep -Fq "$torch" "$doc" || fail "docs/M2.md missing Torch commit"
grep -Fq "boot_main" "$doc" || fail "docs/M2.md missing boot_main"
grep -Fq "GfxRenderingAPI" "$doc" || fail "docs/M2.md missing GfxRenderingAPI"
grep -Fq "never linked" "$doc" || fail "docs/M2.md must keep Torch off ARM11"

version=$(awk '/^#define PB3DS_VERSION / { gsub(/"/, "", $3); print $3 }' \
    "$project_root/include/pb3ds/version.h")
[ "$version" = "0.2.0-m2" ] || fail "expected version 0.2.0-m2, got $version"

if [ "${PB3DS_VERIFY_UPSTREAM:-0}" = "1" ]; then
    check_commit() {
        url=$1
        commit=$2
        curl -fsSIL "$url/commit/$commit" >/dev/null || fail "unreachable $url/commit/$commit"
    }
    check_commit "https://github.com/HarbourMasters/PaperBoat" "$paperboat"
    check_commit "https://github.com/JeodC/libultraship" "$lus"
    check_commit "https://github.com/JeodC/Torch-LH" "$torch"
fi

echo "M2 upstream lock: PaperBoat $release @$paperboat"
