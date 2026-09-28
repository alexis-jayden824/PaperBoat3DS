#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
lock="$project_root/upstream/PAPERBOAT.lock"
root=${PB3DS_PAPERBOAT_ROOT:-"$project_root/.cache/upstream/PaperBoat"}
dest="$root/external/libultraship"
url=$(awk -F= '$1 == "LIBULTRASHIP_REPOSITORY" { print $2 }' "$lock")
commit=$(awk -F= '$1 == "LIBULTRASHIP_COMMIT" { print $2 }' "$lock")

[ -n "$url" ] && [ -n "$commit" ] || { echo "libultraship lock incomplete" >&2; exit 1; }
[ -f "$root/src/main.c" ] || { echo "fetch full PaperBoat first" >&2; exit 1; }
if [ ! -d "$dest/.git" ]; then
    mkdir -p "$dest"
    git -C "$dest" init -q
fi
git -C "$dest" remote remove origin 2>/dev/null || :
git -C "$dest" remote add origin "$url"
git -C "$dest" fetch --quiet --depth 1 origin "$commit"
git -C "$dest" checkout --quiet --detach --force FETCH_HEAD
[ "$(git -C "$dest" rev-parse HEAD)" = "$commit" ] || {
    echo "libultraship commit mismatch" >&2; exit 1;
}
[ -d "$dest/include" ] || { echo "libultraship headers missing" >&2; exit 1; }

# Quoted includes inside LUS resolve next to the header, not via -I. Overlay
# ARM-safe ABI declarations onto the fetched tree without changing the pin.
overlay="$project_root/include/libultraship"
if [ -f "$overlay/libultra.h" ]; then
    cp "$overlay/libultra.h" "$dest/include/libultraship/libultra.h"
    cp "$overlay/libultraship.h" "$dest/include/libultraship/libultraship.h"
    cp "$overlay/libultra/eeprom.h" "$dest/include/libultraship/libultra/eeprom.h"
    cp "$overlay/libultra/interrupt.h" "$dest/include/libultraship/libultra/interrupt.h"
fi
